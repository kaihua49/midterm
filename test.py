# -*- coding: utf-8 -*-
"""
test.py — 雙向摩斯電報器 電腦端測試程式

用法（在 Git Bash）：
    py test.py           自動尋找 ST-LINK 的 COM 埠
    py test.py COM5      手動指定 COM 埠

功能：
    - 背景執行緒持續接收板子傳來的資料，逐字元即時顯示
      （板子按鍵解出的字母沒有換行，所以不能用 readline）
    - 主程式讓使用者輸入一行文字送給板子播放
    - 送出後要等板子回 DONE，才能送下一行
"""

import sys
import threading

import serial
from serial.tools import list_ports

# ======================= 可調參數 =======================
BAUD = 115200            # 鮑率，必須和 CubeMX 的 LPUART1 設定一樣
READ_TIMEOUT_S = 0.1     # 讀取逾時（秒），讓背景執行緒能定期檢查是否該結束
DONE_TIMEOUT_S = 120     # 等 DONE 最多等幾秒（31 字最長大約 70 秒）
MAX_CHARS = 31           # 板子一行最多接受幾個字（和韌體的 RX_BUF_SIZE - 1 一致）


def find_stlink():
    """自動尋找 ST-LINK 虛擬 COM 埠；找不到回傳 None"""
    for p in list_ports.comports():
        # 把描述、製造商、硬體 ID 串在一起，方便搜尋關鍵字
        info = f"{p.description} {p.manufacturer} {p.hwid}"
        if "STLink" in info or "STMicroelectronics" in info:
            return p.device          # 例如 "COM5"
    return None


def list_all_ports():
    """列出電腦上所有 COM 埠，方便使用者手動指定"""
    ports = list(list_ports.comports())
    if not ports:
        print("  （電腦上沒有任何 COM 埠，請確認 USB 線已插好）")
    for p in ports:
        print(f"  {p.device:8s} {p.description}")


def count_valid_chars(text):
    """計算板子會保留的字元數（英文字母、數字、空白）"""
    return sum(1 for c in text if (c.isascii() and c.isalnum()) or c == " ")


def has_playable(text):
    """是否至少有一個可以播放的字元（英文字母或數字）"""
    return any(c.isascii() and c.isalnum() for c in text)


def reader(ser, stop_event, done_event):
    """
    背景執行緒：逐字元讀取並即時印出。
    看到一行內容是 DONE 時，通知主執行緒可以送下一行。
    """
    tail = ""                                   # 記住目前這一行最後幾個字元，用來判斷 DONE
    while not stop_event.is_set():
        try:
            data = ser.read(1)                  # 讀 1 個 byte，最多等 READ_TIMEOUT_S 秒
        except (serial.SerialException, OSError):
            print("\n[錯誤] 序列埠讀取失敗（USB 是否被拔除？）")
            stop_event.set()                    # 通知主執行緒結束
            done_event.set()                    # 避免主執行緒卡在等 DONE
            break

        if not data:                            # 逾時沒收到東西，回去再檢查
            continue

        ch = data.decode("ascii", errors="replace")  # bytes 轉成字元
        if ch == "\r":                          # 忽略 \r，只用 \n 換行
            continue

        print(ch, end="", flush=True)           # 立即印出，不等換行

        if ch == "\n":                          # 一行結束
            if tail.strip().endswith("DONE"):   # 這一行是 DONE
                done_event.set()                # 通知主執行緒
            tail = ""                           # 清空，開始記新的一行
        else:
            tail = (tail + ch)[-16:]            # 只保留最後 16 個字元，避免無限變長


def wait_done(done_event, stop_event, timeout_s):
    """
    等待 DONE。用小段 wait 迴圈，讓 Windows 上按 Ctrl+C 能中斷。
    回傳 True = 收到 DONE；False = 逾時或序列埠已斷線
    """
    waited = 0.0
    while waited < timeout_s:
        if done_event.wait(0.2):                # 每次等 0.2 秒
            return not stop_event.is_set()
        if stop_event.is_set():
            return False
        waited += 0.2
    return False


def main():
    # ---------- 1. 決定 COM 埠 ----------
    if len(sys.argv) > 1:
        port = sys.argv[1]                      # 命令列有指定就用指定的
    else:
        port = find_stlink()                    # 否則自動尋找
        if port is None:
            print("找不到 ST-LINK 的 COM 埠。目前電腦上的埠：")
            list_all_ports()
            print("請用 py test.py COMx 手動指定。")
            sys.exit(1)

    # ---------- 2. 開啟序列埠 ----------
    try:
        ser = serial.Serial(port, BAUD, timeout=READ_TIMEOUT_S)
    except serial.SerialException as e:
        print(f"無法開啟 {port}：{e}")
        print("可能被其他程式佔用（例如 CubeIDE 的序列終端機、TeraTerm），請先關閉。")
        sys.exit(1)

    ser.reset_input_buffer()                    # 清掉開埠前殘留的舊資料

    print(f"已連線 {port} @ {BAUD} bps")
    print("輸入英數字後按 Enter：板子會播放摩斯碼；輸入 exit 離開")
    print("在板子上按 PA0（點）/ PA1（劃），解出的字母會即時顯示在這裡\n", flush=True)

    # ---------- 3. 啟動背景接收執行緒 ----------
    stop_event = threading.Event()              # 通知背景執行緒結束
    done_event = threading.Event()              # 背景執行緒收到 DONE 時設定
    t = threading.Thread(target=reader, args=(ser, stop_event, done_event), daemon=True)
    t.start()                                   # daemon：主程式結束時會自動跟著結束

    # ---------- 4. 主迴圈：讀使用者輸入並送出 ----------
    try:
        while not stop_event.is_set():
            line = input("> ")                  # 等使用者輸入

            if line.strip() == "":              # 空輸入不送
                continue
            if line.strip().lower() == "exit":  # 輸入 exit 離開
                break
            if not has_playable(line):          # 只有標點或中文，板子會當成空行而不回 DONE
                print("（沒有可播放的英數字，未送出）")
                continue
            if count_valid_chars(line) > MAX_CHARS:
                print(f"（超過 {MAX_CHARS} 字，板子只會播放前 {MAX_CHARS} 字）")

            done_event.clear()                  # 先清除，準備等這一行的 DONE
            ser.write((line + "\n").encode("utf-8"))  # 送出，以 \n 結尾
            ser.flush()                         # 確定資料真的送出去

            if not wait_done(done_event, stop_event, DONE_TIMEOUT_S):  # 等 DONE 才能送下一行
                if stop_event.is_set():
                    break
                print(f"\n（等了 {DONE_TIMEOUT_S} 秒沒收到 DONE，請檢查板子）")

    except (KeyboardInterrupt, EOFError):       # Ctrl+C 或輸入結束
        print("\n使用者中斷")

    finally:
        # ---------- 5. 確實關閉 ----------
        stop_event.set()                        # 通知背景執行緒停止
        t.join(timeout=1.0)                     # 最多等它 1 秒
        if ser.is_open:
            ser.close()                         # 關閉序列埠，釋放 COM 埠
        print("序列埠已關閉，程式結束")


if __name__ == "__main__":
    main()