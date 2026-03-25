#include "NUC100Series.h"
#include "RC522.h"
#include "LCD.h"
#include <stdio.h>
#include <string.h>

extern void SYS_Init(void); 

int main(void)
{
    uint8_t UID[5];
    uint8_t cardType[2];
    char lcd_buffer[16]; 
    char uid_str[10];
    uint8_t version;
    char esp_response[10];
    uint32_t timeout_cnt;
    uint8_t rx_idx;
    char send_buffer[32]; // Khai báo tại đầu hàm để tránh lỗi C89
    
    // System and Peripheral Initialization
    SYS_Init();

    // Khởi tạo thư viện hiển thị
    init_LCD();
    clear_LCD();
    print_Line(0, "   RFID ESP32  "); 
    
    // Bật clock cho UART0
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART_S_HIRC, CLK_CLKDIV_UART(1));

    // Cấu hình chân UART0: PB.0 (RX) và PB.1 (TX)
    SYS->GPB_MFP &= ~(SYS_GPB_MFP_PB0_Msk | SYS_GPB_MFP_PB1_Msk);
    SYS->GPB_MFP |= (SYS_GPB_MFP_PB0_UART0_RXD | SYS_GPB_MFP_PB1_UART0_TXD);

    // Mở cổng UART0 với tốc độ 9600 (an toàn hơn, đỡ lỗi clock)
    UART_Open(UART0, 9600);
    
    printf("Nuvoton RFID System Started!\n");
    print_Line(3, "Baud: 9600      "); 
    
    // Thiết lập duy nhất: SPI1 dành riêng cho thẻ RC522 (dùng AutoSS trên PC.8)
    SPI_Open(SPI1, SPI_MASTER, SPI_MODE_0, 8, 2000000);
    SPI_EnableAutoSS(SPI1, SPI_SS0, SPI_SS_ACTIVE_LOW);
    
    // Initialize RC522 (SPI1)
    RC522_Init();
    
    version = RC522_ReadReg(0x37); 
    sprintf(lcd_buffer, "RC522 Ver: %02X", version);
    print_Line(1, lcd_buffer); 
    print_Line(2, "WiFi: ESP32 Mngr"); // ESP32 sẽ tự quản lý việc kết nối WiFi, Nuvoton không quan tâm nữa.
    CLK_SysTickDelay(2000000); 

    print_Line(1, "Dang quet the..."); 
    print_Line(2, "                ");

    while(1)
    {
        // 1. Quét thẻ liên tục
        if (RC522_Request(PICC_REQIDL, cardType) == MI_OK) 
        {
            // 2. Đọc mã thẻ
            if (RC522_Anticoll(UID) == MI_OK) 
            {
                sprintf(uid_str, "%02X%02X%02X%02X", UID[0], UID[1], UID[2], UID[3]);
                sprintf(lcd_buffer, "UID:%s", uid_str);
                print_Line(2, lcd_buffer);         
                
                print_Line(3, "A: Clears UART  "); 
                // 3. Xoá rác trong bộ đệm UART (những thứ tồn đọng do nhiễu hoặc ESP32 gửi thừa)
                while (!UART_GET_RX_EMPTY(UART0)) {
                    UART_READ(UART0);
                }
                
                print_Line(3, "B: Sending...   "); 
                // 4. Gửi UID sang ESP32 qua UART0
                // Sử dụng UART_Write để đảm bảo dữ liệu đi thẳng ra chân vật lý
                sprintf(send_buffer, "UID:%s\n", uid_str);
                UART_Write(UART0, (uint8_t *)send_buffer, strlen(send_buffer));
                
                print_Line(3, "C: Waiting ESP.."); 
                // 5. Chờ phản hồi từ ESP32 trong khoảng ~2.5 giây. ESP32 sẽ gửi "OK\n" hoặc "ERR\n"
                timeout_cnt = 25000; // Delay 100us * 25000 = ~2.5s. Web API nếu mạng chậm sẽ mất đến 2s.
                rx_idx = 0;
                memset(esp_response, 0, sizeof(esp_response));
                
                while(timeout_cnt > 0) {
                    if(!UART_GET_RX_EMPTY(UART0)) {
                        char c = UART_READ(UART0);
                        if(c == '\n' || c == '\r') {
                            if (rx_idx > 0) break; // Đoạn này bắt dấu xuống dòng (đã nhận đủ message)
                        } else {
                            if(rx_idx < sizeof(esp_response)-1) {
                                esp_response[rx_idx++] = c;
                            }
                        }
                    } else {
                        CLK_SysTickDelay(100);
                        timeout_cnt--;
                    }
                }
                
                // 6. Xử lý logic hiển thị LCD tùy vào ESP32 trả về cái gì
                if (rx_idx > 0) {
                    if (strncmp(esp_response, "OK", 2) == 0) {
                        print_Line(3, ">> Web OK!      "); // Server trả về code 200
                    } else if (strncmp(esp_response, "ERR", 3) == 0) {
                        print_Line(3, ">> X Web loi!   "); // Trả về lỗi 404, 500, lỗi mạng v.v...
                    } else {
                        print_Line(3, ">> X ESP loi!   "); // Data ESP32 gửi về không đúng chuẩn "OK" hay "ERR"
                    }
                } else {
                    print_Line(3, ">> Timeout ESP! "); // Chờ mãi không thấy ESP32 rep 
                }
                
                // Tạm dừng 2s để người dùng dễ quan sát trên màn hình rồi sang lần quẹt tiếp theo
                CLK_SysTickDelay(2000000); 
                print_Line(3, "                "); 
            }
        }
        CLK_SysTickDelay(100000); 
    }
}
