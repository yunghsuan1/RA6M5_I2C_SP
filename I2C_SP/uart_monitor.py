#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import time
import queue
import threading
import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports

class SerialMonitorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("RA6M5 I2C 雙端協定偵錯終端機 (Dual-Port Serial Monitor)")
        self.root.geometry("1100x750")
        self.root.minsize(800, 550)

        # 設定外觀色彩系統 (Sleek Dark Theme, with High-Contrast controls)
        self.bg_color = "#1e1e2e"
        self.fg_color = "#cdd6f4"
        self.term_bg = "#11111b"
        self.term_fg = "#a6e3a1"      # 經典終端綠
        self.accent_color = "#89b4fa"  # 淡藍
        self.panel_bg = "#181825"
        self.btn_bg = "#313244"
        self.btn_fg = "#cdd6f4"

        # 設定下拉選單彈出視窗的顏色，確保在所有平台上皆清晰可見 (高對比黑白)
        self.root.option_add('*TCombobox*Listbox.background', 'white')
        self.root.option_add('*TCombobox*Listbox.foreground', 'black')
        self.root.option_add('*TCombobox*Listbox.selectBackground', '#89b4fa')
        self.root.option_add('*TCombobox*Listbox.selectForeground', 'black')

        # 初始化樣式
        self.setup_styles()

        # 每個 Port 的佇列 (Queue) 與狀態
        self.queues = {
            "master": queue.Queue(),
            "slave": queue.Queue()
        }
        
        self.serial_threads = {
            "master": None,
            "slave": None
        }
        
        self.serial_ports = {
            "master": None,
            "slave": None
        }
        
        self.running_flags = {
            "master": threading.Event(),
            "slave": threading.Event()
        }

        self.auto_scroll = {
            "master": tk.BooleanVar(value=True),
            "slave": tk.BooleanVar(value=True)
        }

        # 建立 UI 配置
        self.create_widgets()
        
        # 開始輪詢接收佇列
        self.poll_queues()
        
        # 初始刷新 COM 埠列表
        self.refresh_ports()

    def setup_styles(self):
        self.root.configure(bg=self.bg_color)
        
        style = ttk.Style()
        style.theme_use('clam')
        
        # 自訂 Frame 樣式
        style.configure("TFrame", background=self.bg_color)
        style.configure("Panel.TFrame", background=self.panel_bg)
        
        # 自訂 Label 樣式
        style.configure("TLabel", background=self.bg_color, foreground=self.fg_color)
        style.configure("Header.TLabel", font=("Microsoft JhengHei", 12, "bold"), background=self.panel_bg, foreground=self.accent_color)
        style.configure("Panel.TLabel", background=self.panel_bg, foreground=self.fg_color)
        
        # 自訂 Checkbutton
        style.configure("TCheckbutton", background=self.panel_bg, foreground=self.fg_color)
        
        # 自訂 Combobox 控制項（白底黑字，確保看得到）
        style.configure("TCombobox", 
                        fieldbackground="white", 
                        background="#e0e0e0", 
                        foreground="black", 
                        arrowcolor="black")
        
        # 自訂 Entry 控制項 (白底黑字)
        style.configure("TEntry", fieldbackground="white", foreground="black")
        
        # 自訂 Button
        style.configure("TButton", background=self.btn_bg, foreground=self.btn_fg, borderwidth=1, focuscolor=self.accent_color)
        style.map("TButton",
                  background=[('active', self.accent_color), ('disabled', '#45475a')],
                  foreground=[('active', '#11111b'), ('disabled', '#7f849c')])

    def create_widgets(self):
        # 標題與上方工具列
        top_bar = ttk.Frame(self.root, style="TFrame")
        top_bar.pack(fill=tk.X, padx=15, pady=10)
        
        title_label = ttk.Label(top_bar, text="RA6M5 I2C Packet Protocol Debugger", font=("Consolas", 16, "bold"), background=self.bg_color, foreground=self.accent_color)
        title_label.pack(side=tk.LEFT)
        
        refresh_btn = ttk.Button(top_bar, text="🔄 重新整理 COM 埠", command=self.refresh_ports)
        refresh_btn.pack(side=tk.RIGHT, padx=5)

        # 主顯示區（左右分欄）
        main_pane = ttk.Frame(self.root, style="TFrame")
        main_pane.pack(fill=tk.BOTH, expand=True, padx=15, pady=5)
        
        # 設定 grid 比重使得左右平分
        main_pane.grid_columnconfigure(0, weight=1)
        main_pane.grid_columnconfigure(1, weight=1)
        main_pane.grid_rowconfigure(0, weight=1)

        # === 左側 Master 監控面版 ===
        self.master_frame = self.build_console_panel(main_pane, "master", "Master Console (UART9 - 發送端)")
        self.master_frame.grid(row=0, column=0, sticky="nsew", padx=(0, 8))

        # === 右側 Slave 監控面版 ===
        self.slave_frame = self.build_console_panel(main_pane, "slave", "Slave Console (UART8 - 接收端)")
        self.slave_frame.grid(row=0, column=1, sticky="nsew", padx=(8, 0))

        # 下方狀態列
        self.status_bar = tk.Label(self.root, text="歡迎使用！請選擇 COM 埠並點擊連線。", bd=1, relief=tk.SUNKEN, anchor=tk.W, bg=self.panel_bg, fg="#a6adc8", font=("Microsoft JhengHei", 9))
        self.status_bar.pack(fill=tk.X, side=tk.BOTTOM, ipady=3)

    def build_console_panel(self, parent, side_id, title):
        frame = ttk.Frame(parent, style="Panel.TFrame", padding=10)
        
        # 面版標題與狀態指示器
        header_frame = ttk.Frame(frame, style="Panel.TFrame")
        header_frame.pack(fill=tk.X, pady=(0, 10))
        
        title_lbl = ttk.Label(header_frame, text=title, style="Header.TLabel")
        title_lbl.pack(side=tk.LEFT)
        
        # 連線狀態燈號
        status_dot = tk.Canvas(header_frame, width=12, height=12, bg=self.panel_bg, highlightthickness=0)
        status_dot.pack(side=tk.LEFT, padx=10)
        status_dot.create_oval(2, 2, 10, 10, fill="#f38ba8", tags="dot") # 預設紅色代表未連線
        setattr(self, f"{side_id}_dot", status_dot)

        # 控制項區域 (COM Port, Baudrate, Connect button)
        ctrl_frame = ttk.Frame(frame, style="Panel.TFrame")
        ctrl_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(ctrl_frame, text="埠號:", style="Panel.TLabel").pack(side=tk.LEFT)
        port_combo = ttk.Combobox(ctrl_frame, width=10, state="readonly")
        port_combo.pack(side=tk.LEFT, padx=5)
        setattr(self, f"{side_id}_port", port_combo)
        
        ttk.Label(ctrl_frame, text="鮑率:", style="Panel.TLabel").pack(side=tk.LEFT, padx=(10, 0))
        baud_combo = ttk.Combobox(ctrl_frame, width=8, state="readonly", values=["9600", "19200", "38400", "57600", "115200", "921600"])
        baud_combo.set("115200")
        baud_combo.pack(side=tk.LEFT, padx=5)
        setattr(self, f"{side_id}_baud", baud_combo)
        
        conn_btn = ttk.Button(ctrl_frame, text="連線", width=8, command=lambda: self.toggle_connection(side_id))
        conn_btn.pack(side=tk.LEFT, padx=10)
        setattr(self, f"{side_id}_btn", conn_btn)

        # 終端機控制鈕 (Clear, Auto Scroll)
        terminal_ctrls = ttk.Frame(frame, style="Panel.TFrame")
        terminal_ctrls.pack(fill=tk.X, pady=(0, 5))
        
        clear_btn = ttk.Button(terminal_ctrls, text="🧹 清除視窗", command=lambda: self.clear_terminal(side_id))
        clear_btn.pack(side=tk.LEFT)
        
        scroll_cb = ttk.Checkbutton(terminal_ctrls, text="自動捲動", variable=self.auto_scroll[side_id])
        scroll_cb.pack(side=tk.RIGHT, padx=5)

        # 終端機文字顯示區 (附 Scrollbar)
        text_frame = ttk.Frame(frame, style="Panel.TFrame")
        text_frame.pack(fill=tk.BOTH, expand=True)
        
        scrollbar = ttk.Scrollbar(text_frame)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # 採用寬鬆行高與等寬字型，深色底綠色字
        text_widget = tk.Text(text_frame, wrap=tk.WORD, yscrollcommand=scrollbar.set,
                              bg=self.term_bg, fg=self.term_fg, 
                              insertbackground=self.fg_color,
                              font=("Consolas", 10),
                              padx=5, pady=5)
        text_widget.pack(fill=tk.BOTH, expand=True, side=tk.LEFT)
        text_widget.configure(state=tk.DISABLED) # 唯讀
        scrollbar.config(command=text_widget.yview)
        setattr(self, f"{side_id}_text", text_widget)

        # 指令傳送區
        send_frame = ttk.Frame(frame, style="Panel.TFrame")
        send_frame.pack(fill=tk.X, pady=(10, 0))
        
        ttk.Label(send_frame, text="發送指令:", style="Panel.TLabel").pack(side=tk.LEFT, padx=(0, 5))
        
        # 建立一個白底黑字的 Entry
        cmd_entry = tk.Entry(send_frame, bg="white", fg="black", insertbackground="black", font=("Consolas", 10))
        cmd_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        setattr(self, f"{side_id}_entry", cmd_entry)
        
        send_btn = ttk.Button(send_frame, text="發送", width=8, command=lambda: self.send_command(side_id))
        send_btn.pack(side=tk.RIGHT)
        setattr(self, f"{side_id}_send_btn", send_btn)
        
        # 綁定 Enter 鍵觸發發送
        cmd_entry.bind("<Return>", lambda event: self.send_command(side_id))
        
        return frame

    def refresh_ports(self):
        """掃描系統中可用的 COM 埠"""
        ports = serial.tools.list_ports.comports()
        port_list = [p.device for p in ports]
        
        # 更新兩個 ComboBox
        for side in ["master", "slave"]:
            combo = getattr(self, f"{side}_port")
            current = combo.get()
            combo['values'] = port_list
            if port_list:
                if current in port_list:
                    combo.set(current)
                else:
                    combo.set(port_list[0])
            else:
                combo.set("")
        
        self.update_status(f"已重新整理 COM 埠列表，發現 {len(port_list)} 個可用埠。")

    def toggle_connection(self, side_id):
        """開啟或關閉指定端點的序列連線"""
        if self.running_flags[side_id].is_set():
            # 已連線，執行中斷連線
            self.disconnect_port(side_id)
        else:
            # 未連線，執行連線
            self.connect_port(side_id)

    def connect_port(self, side_id):
        port = getattr(self, f"{side_id}_port").get()
        baud = getattr(self, f"{side_id}_baud").get()
        
        if not port:
            messagebox.showwarning("警告", "請選擇正確的 COM 埠號！")
            return
            
        try:
            ser = serial.Serial(
                port=port,
                baudrate=int(baud),
                timeout=0.1,
                xonxoff=False,
                rtscts=False,
                dsrdtr=False
            )
            self.serial_ports[side_id] = ser
            self.running_flags[side_id].set()
            
            # 建立背景執行緒讀取資料
            t = threading.Thread(target=self.read_serial_loop, args=(side_id, ser), daemon=True)
            self.serial_threads[side_id] = t
            t.start()
            
            # 更新 UI 狀態
            getattr(self, f"{side_id}_btn").config(text="中斷")
            getattr(self, f"{side_id}_port").config(state="disabled")
            getattr(self, f"{side_id}_baud").config(state="disabled")
            
            # 指示燈設為綠色
            dot = getattr(self, f"{side_id}_dot")
            dot.itemconfig("dot", fill="#a6e3a1")
            
            self.write_to_terminal_local(side_id, f"--- 已成功連線至 {port} ({baud}bps) ---\n")
            self.update_status(f"[{side_id.upper()}] 已連線至 {port}")
            
        except Exception as e:
            messagebox.showerror("連線失敗", f"無法開啟 {port}，錯誤資訊：\n{str(e)}")
            self.disconnect_port(side_id)

    def disconnect_port(self, side_id):
        # 設旗標停止執行緒
        self.running_flags[side_id].clear()
        
        # 關閉序列埠
        ser = self.serial_ports[side_id]
        if ser and ser.is_open:
            try:
                ser.close()
            except Exception:
                pass
        self.serial_ports[side_id] = None
        
        # 等待執行緒結束
        t = self.serial_threads[side_id]
        if t and t.is_alive():
            t.join(timeout=0.5)
        self.serial_threads[side_id] = None
        
        # 更新 UI
        getattr(self, f"{side_id}_btn").config(text="連線")
        getattr(self, f"{side_id}_port").config(state="readonly")
        getattr(self, f"{side_id}_baud").config(state="readonly")
        
        # 指示燈設為紅色
        dot = getattr(self, f"{side_id}_dot")
        dot.itemconfig("dot", fill="#f38ba8")
        
        self.write_to_terminal_local(side_id, "\n--- 序列埠連線已結束 ---\n")
        self.update_status(f"[{side_id.upper()}] 已關閉連線。")

    def read_serial_loop(self, side_id, ser):
        """背景序列埠讀取執行緒迴圈"""
        while self.running_flags[side_id].is_set():
            if not ser.is_open:
                break
            try:
                # 批次讀取可用字節
                waiting = ser.in_waiting
                if waiting > 0:
                    data = ser.read(waiting)
                    if data:
                        # 將接收字節送入佇列 (以帶容錯解碼方式解成 string)
                        text_chunk = data.decode('utf-8', errors='replace')
                        self.queues[side_id].put(text_chunk)
                else:
                    time.sleep(0.01) # 避開 CPU 滿載
            except Exception as e:
                # 處理連線中途斷開（如 USB 拔除）
                self.queues[side_id].put(f"\n[錯誤] 序列埠連線異常中斷: {str(e)}\n")
                # 安排在主執行緒執行 UI 關閉連線
                self.root.after_idle(lambda: self.disconnect_port(side_id))
                break

    def send_command(self, side_id):
        """傳送指令至開發板"""
        entry = getattr(self, f"{side_id}_entry")
        cmd_text = entry.get().strip()
        if not cmd_text:
            return
            
        ser = self.serial_ports[side_id]
        if not ser or not self.running_flags[side_id].is_set():
            messagebox.showwarning("警告", "請先連線序列埠後再傳送指令！")
            return
            
        try:
            # 加上換行符 \n 送出
            send_data = (cmd_text + "\n").encode('utf-8')
            ser.write(send_data)
            
            # 在本地終端機回顯送出的指令 (以白色或特殊格式標記)
            self.write_to_terminal_local(side_id, f">> {cmd_text}\n")
            
            # 清空輸入框
            entry.delete(0, tk.END)
        except Exception as e:
            messagebox.showerror("傳送失敗", f"傳送指令失敗，錯誤資訊：\n{str(e)}")

    def poll_queues(self):
        """定時輪詢接收佇列並顯示在 UI"""
        for side in ["master", "slave"]:
            q = self.queues[side]
            text_widget = getattr(self, f"{side}_text")
            
            # 一次性處理所有排隊資料，避免阻塞 UI
            has_data = False
            chunks = []
            while not q.empty():
                try:
                    chunks.append(q.get_nowait())
                    has_data = True
                except queue.Empty:
                    break
            
            if has_data:
                full_text = "".join(chunks)
                text_widget.configure(state=tk.NORMAL)
                text_widget.insert(tk.END, full_text)
                
                # 限製字元長度防止記憶體過大 (保留最後 50,000 字元)
                content_len = len(text_widget.get("1.0", tk.END))
                if content_len > 100000:
                    text_widget.delete("1.0", f"end-50000c")
                
                text_widget.configure(state=tk.DISABLED)
                
                if self.auto_scroll[side].get():
                    text_widget.see(tk.END)
                    
        # 每 30ms 輪詢一次
        self.root.after(30, self.poll_queues)

    def write_to_terminal_local(self, side_id, message):
        """本地寫入系統訊息到終端機介面"""
        self.queues[side_id].put(message)

    def clear_terminal(self, side_id):
        """清空終端機視窗"""
        text_widget = getattr(self, f"{side_id}_text")
        text_widget.configure(state=tk.NORMAL)
        text_widget.delete("1.0", tk.END)
        text_widget.configure(state=tk.DISABLED)
        self.update_status(f"已清除 [{side_id.upper()}] 終端機顯示內容。")

    def update_status(self, msg):
        self.status_bar.config(text=f"狀態: {msg}")

if __name__ == "__main__":
    # 確保安裝 pyserial 模組
    try:
        import serial
    except ImportError:
        print("未安裝 pyserial 模組！請執行: pip install pyserial")
        # 啟動 Tk 提示訊息
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("錯誤", "找不到 pyserial 模組！\n\n請先打開命令提示字元並執行以下指令安裝：\npip install pyserial")
        sys.exit(1)
        
    root = tk.Tk()
    app = SerialMonitorApp(root)
    
    # 程式退出關閉所有序列埠
    def on_closing():
        for side in ["master", "slave"]:
            if app.running_flags[side].is_set():
                app.disconnect_port(side)
        root.destroy()
        
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()
