#pragma once

// Đăng ký các route và khởi động HTTP server
void httpServerSetup();

// Gọi trong loop() để xử lý request đến
void httpServerLoop();
