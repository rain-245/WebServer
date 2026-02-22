// test.cgi 代码
#include <stdio.h>
int main() {
    // 必须先输出 HTTP 响应头，空行分隔头和内容
    printf("Content-Type: text/html; charset=utf-8\r\n");
    printf("\r\n");
    // 输出动态内容
    printf("<html><head><title>CGI Test</title></head>");
    printf("<body><h1>CGI 服务器运行成功！</h1>");
    printf("<p>这是测试 CGI 程序的输出</p></body></html>");
    return 0;
}