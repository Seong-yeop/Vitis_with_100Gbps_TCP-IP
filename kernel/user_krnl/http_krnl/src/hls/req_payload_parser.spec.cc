#include <iostream>
#include <cstring>
#include <hls_stream.h>
#include "req_payload_parser.h"

using namespace std;
using namespace http;

// Function to populate the HTTP request in the input stream
void populate_http_request(hls::stream<axi_stream_ispt>& input_stream) {
    axi_stream_ispt packet;

    // HTTP Header lines
    const char* header1 = "POST /test HTTP/1.1\r\n";
    const char* header2 = "Host: example.com\r\n";
    const char* header3 = "Content-Length: 13\r\n";
    const char* header4 = "\r\n";  // End of headers

    // HTTP Body
    const char* body = "Hello, World!";

    // Populate header1
    packet.data = 0;
    memcpy(&packet.data, header1, strlen(header1));
    packet.last = false;
    input_stream.write(packet);

    // Populate header2
    packet.data = 0;
    memcpy(&packet.data, header2, strlen(header2));
    packet.last = false;
    input_stream.write(packet);

    // Populate header3
    packet.data = 0;
    memcpy(&packet.data, header3, strlen(header3));
    packet.last = false;
    input_stream.write(packet);

    // Populate header4 (end of headers)
    packet.data = 0;
    memcpy(&packet.data, header4, strlen(header4));
    packet.last = true;
    input_stream.write(packet);

    // Populate body (single packet for simplicity)
    packet.data = 0;
    memcpy(&packet.data, body, strlen(body));
    packet.last = true; // Indicating the end of the body
    input_stream.write(packet);
}

int main() {
    // Define the streams
    hls::stream<axi_stream_ispt> input_stream;
    hls::stream<pkt512> header_stream;
    hls::stream<pkt512> body_stream;

    // Populate the input stream with HTTP request data
    populate_http_request(input_stream);

    // Call the function under test (FUT) multiple times to simulate the FSM processing
    for (int i = 0; i < 10; ++i) {
        req_payload_parser(input_stream, header_stream, body_stream);
    }

    // Check header output
    cout << "Header Packets:" << endl;
    while (!header_stream.empty()) {
        pkt512 header_pkt = header_stream.read();
        cout << "Header Packet: " << hex << header_pkt.data << endl;
    }

    // Check body output
    cout << "Body Packets:" << endl;
    while (!body_stream.empty()) {
        pkt512 body_pkt = body_stream.read();
        cout << "Body Packet: " << hex << body_pkt.data << endl;
    }

    return 0;
}
