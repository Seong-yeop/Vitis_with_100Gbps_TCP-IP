#include "listen_port.h"


namespace http {

void listen_port (
    hls::stream<ap_uint<16>>& listenPort,
    hls::stream<bool>& listenPortStatus,
    ap_uint<16> port_number
) {
    #pragma HLS dataflow disable_start_propagation

    listenPort.write(port_number);

    bool listen_rsp_pkt;

    if (!listenPortStatus.empty()) {
        listenPortStatus.read(listen_rsp_pkt);
    }
    // listenPortStatus.read(listen_rsp_pkt);
}

} // namespace http