# Hướng Dẫn Cập Nhật Firmware CrossPoint (Tiếng Việt)

## Yêu Cầu

- Cáp USB-C
- Máy tính (Windows / macOS / Linux)
- File `firmware.bin` (tải từ bản release mới nhất)

---

## Cách 1: Cập nhật qua WiFi (OTA) — Dễ nhất

1. Vào **Settings → System → Check for updates**
2. Kết nối WiFi nếu chưa kết nối
3. Thiết bị tự tải và cài firmware mới
4. Chờ thiết bị khởi động lại

> Nếu không có kết nối WiFi hoặc muốn cài thủ công, dùng Cách 2.

---

## Cách 2: Flash thủ công bằng esptool

### Bước 1 — Cài Python và esptool

```bash
pip install esptool
```

### Bước 2 — Kết nối thiết bị

Cắm cáp USB-C vào thiết bị và máy tính.

Tìm cổng COM (Windows) hoặc `/dev/cu.*` / `/dev/ttyUSB*` (macOS/Linux):

```bash
# macOS/Linux
ls /dev/cu.usb* /dev/ttyUSB* 2>/dev/null

# Windows (PowerShell)
[System.IO.Ports.SerialPort]::GetPortNames()
```

### Bước 3 — Flash firmware

```bash
esptool.py --chip esp32c3 --port <CỔNG> --baud 921600 \
  write_flash -z 0x10000 firmware.bin
```

Thay `<CỔNG>` bằng cổng tìm được ở Bước 2.

**Ví dụ:**

```bash
# macOS
esptool.py --chip esp32c3 --port /dev/cu.usbmodem101 --baud 921600 \
  write_flash -z 0x10000 firmware.bin

# Windows
esptool.py --chip esp32c3 --port COM3 --baud 921600 \
  write_flash -z 0x10000 firmware.bin
```

### Bước 4 — Khởi động lại thiết bị

Nhấn nút **Reset** hoặc rút và cắm lại cáp USB.

---

## Khắc Phục Sự Cố

| Vấn đề | Giải pháp |
|--------|-----------|
| Không tìm thấy cổng COM | Cài driver CH340/CP210x, thử cáp khác |
| Lỗi `Failed to connect` | Giữ nút Boot khi cắm USB, thử lại |
| Thiết bị bị treo (bootloop) | Nhấn Reset, rồi giữ nút Back + Power để vào Home |
| Màn hình trắng sau flash | Xóa thư mục `.crosspoint` trên thẻ SD và khởi động lại |

---

## Ghi Chú Về Thẻ SD

Dữ liệu đọc sách và cài đặt được lưu trên thẻ SD — **không bị mất** khi cập nhật firmware.

Nếu muốn reset hoàn toàn, xóa file `.crosspoint/settings.bin` trên thẻ SD.

---

## Phiên Bản Mới Nhất

**v1.2.0** — Thú cưng ảo nâng cấp, đồng hồ ngủ hiện đại, mèo pixel art mới.

Xem lịch sử phiên bản tại: `CHANGELOG.md` hoặc trang GitHub releases.
