/************************************************
Copyright (c) 2019, Systems Group, ETH Zurich.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************/
#include <iostream>

#include "http_client_config.hpp"
#include "http_client.hpp"
#include "listen_port.h"
#include "request_processor.h"
#include "../../../../common/include/communication.hpp"

//Buffers responses coming from the TCP stack
void status_handler(hls::stream<appTxRsp>&				txStatus,
							hls::stream<internalAppTxRsp>&	txStatusBuffer)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	if (!txStatus.empty())
	{
		appTxRsp resp = txStatus.read();
		txStatusBuffer.write(internalAppTxRsp(resp.sessionID, resp.error));
	}
}

//Buffers open status coming from the TCP stack
void openStatus_handler(hls::stream<openStatus>&				openConStatus,
							hls::stream<openStatus>&	openConStatusBuffer)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	if (!openConStatus.empty())
	{
		openStatus resp = openConStatus.read();
		openConStatusBuffer.write(resp);
	}
}


void txMetaData_handler(hls::stream<appTxMeta>&	txMetaDataBuffer, 
							hls::stream<appTxMeta>& txMetaData)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	if (!txMetaDataBuffer.empty())
	{
		appTxMeta metaDataReq = txMetaDataBuffer.read();
		txMetaData.write(metaDataReq);
	}
}

template <int WIDTH>
void txDataBuffer_handler(hls::stream<net_axis<WIDTH> >& txDataBuffer,
							hls::stream<ap_axiu<WIDTH, 0, 0, 0> >& txData)
{
	#pragma HLS PIPELINE II=1
	#pragma HLS INLINE off

	if (!txDataBuffer.empty())
	{
		net_axis<WIDTH> inWord = txDataBuffer.read();
		ap_axiu<WIDTH, 0, 0, 0> outWord;
		outWord.data = inWord.data;
		outWord.keep = inWord.keep;
		outWord.last = inWord.last;
		txData.write(outWord);
	}
}

template <int WIDTH>
void rxDataBuffer_handler(hls::stream<ap_axiu<WIDTH, 0, 0, 0> >& rxData,
						hls::stream<net_axis<WIDTH> >& rxDataBuffer)
{
	#pragma HLS PIPELINE II=1
	#pragma HLS INLINE off

	if (!rxData.empty())
	{
		ap_axiu<WIDTH, 0, 0, 0> inWord = rxData.read();
		net_axis<WIDTH> outWord;
		outWord.data = inWord.data;
		outWord.keep = inWord.keep;
		outWord.last = inWord.last;
		rxDataBuffer.write(outWord);
	}
}


template <int WIDTH>
void client(hls::stream<ipTuple>& openConnection,
                 hls::stream<openStatus>& openConStatusBuffer,
                 hls::stream<ap_uint<16>>& closeConnection,
                 hls::stream<appTxMeta>& txMetaDataBuffer,
                 hls::stream<net_axis<WIDTH>>& txDataBuffer,
                 hls::stream<internalAppTxRsp>& txStatus,
                 hls::stream<bool>& startSignal,
                 hls::stream<bool>& stopSignal,
                 ap_uint<1> runExperiment,
                 ap_uint<1> dualModeEn,
                 ap_uint<14> useConn,
                 ap_uint<16> useIpAddr, // Total IP addresses used
                 ap_uint<8> pkgWordCount,
                 ap_uint<8> packetGap,
                 ap_uint<32> timeInSeconds,
                 ap_uint<16> regBasePort,
                 ap_uint<32> regIpAddress0,
                 ap_uint<32> regIpAddress1,
                 ap_uint<32> regIpAddress2,
                 ap_uint<32> regIpAddress3,
                 ap_uint<32> regIpAddress4,
                 ap_uint<32> regIpAddress5,
                 ap_uint<32> regIpAddress6,
                 ap_uint<32> regIpAddress7,
                 ap_uint<32> regIpAddress8,
                 ap_uint<32> regIpAddress9) {
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

    enum clientFsmStateType {IDLE, INIT_CON, WAIT_CON, SEND_REQUEST, SEND_BODY, CHECK_TIME};
    static clientFsmStateType clientFsmState = IDLE;

    static ap_uint<14> numConnections = 0;
    static ap_uint<16> currentSessionID;
    static ap_uint<14> sessionIt = 0;
    static ap_uint<14> closeIt = 0;
    static bool timeOver = false;
    static bool stopSend = false;

    static ap_uint<4> ipAddressIdx = 0;
    static ap_uint<8> packetGapCounter = 0;

    switch (clientFsmState) {
        case IDLE:
            sessionIt = 0;
            closeIt = 0;
            numConnections = 0;
            timeOver = false;
            ipAddressIdx = 0;
            stopSend = false;
            if (runExperiment) {
                clientFsmState = INIT_CON;
            }
            break;
        case INIT_CON:
            if (sessionIt < useConn) {
                ipTuple openTuple;
                switch (ipAddressIdx) {
                    case 0: openTuple.ip_address = regIpAddress0; break;
                    case 1: openTuple.ip_address = regIpAddress1; break;
                    case 2: openTuple.ip_address = regIpAddress2; break;
                    case 3: openTuple.ip_address = regIpAddress3; break;
                    case 4: openTuple.ip_address = regIpAddress4; break;
                    case 5: openTuple.ip_address = regIpAddress5; break;
                    case 6: openTuple.ip_address = regIpAddress6; break;
                    case 7: openTuple.ip_address = regIpAddress7; break;
                    case 8: openTuple.ip_address = regIpAddress8; break;
                    case 9: openTuple.ip_address = regIpAddress9; break;
                }
                openTuple.ip_port = 80; // HTTP port
                openConnection.write(openTuple);
                ipAddressIdx++;
                if (ipAddressIdx == useIpAddr) {
                    ipAddressIdx = 0;
                }
            }
            sessionIt++;
			// std::cout << "Connection " << sessionIt << " opened." << std::endl;
            if (sessionIt == useConn) {
                sessionIt = 0;
                clientFsmState = WAIT_CON;
            }
			break;
		case WAIT_CON: 
			// std::cout << "Waiting for connection to open." << std::endl;
            if (!openConStatusBuffer.empty()) {
				std::cout << "Connection opened." << std::endl;
                openStatus status = openConStatusBuffer.read();
                if (status.success) {
                    std::cout << "Connection successfully opened." << std::endl;
                    txMetaDataBuffer.write(appTxMeta(status.sessionID, 256)); // Length for header
                    currentSessionID = status.sessionID;
                    clientFsmState = SEND_REQUEST;
                } else {
                    std::cout << "Connection could not be opened." << std::endl;
                }
            }
            break;
        case SEND_REQUEST: {
			std::cout << "Sending request." << std::endl;
            // HTTP Request Header
            const char* header = "POST /test HTTP/1.1\r\nHost: example.com\r\nContent-Length: 13\r\n\r\n";
            int headerLen = strlen(header);
            net_axis<WIDTH> headerWord;
            
            for (int i = 0; i < (headerLen / (WIDTH / 8)) + 1; i++) {
                #pragma HLS UNROLL
                memset(&headerWord.data, 0, sizeof(headerWord.data)); // Clear data
                int offset = i * (WIDTH / 8);
                int len = std::min((int)(WIDTH / 8), headerLen - offset);
                memcpy(&headerWord.data, header + offset, len);
                headerWord.keep = (len == (WIDTH / 8)) ? -1 : (1 << len) - 1;
                headerWord.last = (i == (headerLen / (WIDTH / 8)));
                txDataBuffer.write(headerWord);
            }

            clientFsmState = SEND_BODY;
            break;
        }
        case SEND_BODY: {
            // HTTP Request Body
            const char* body = "Hello, World!";
            int bodyLen = strlen(body);
            net_axis<WIDTH> bodyWord;

            for (int i = 0; i < (bodyLen / (WIDTH / 8)) + 1; i++) {
                #pragma HLS UNROLL
                memset(&bodyWord.data, 0, sizeof(bodyWord.data)); // Clear data
                int offset = i * (WIDTH / 8);
                int len = std::min((int)(WIDTH / 8), bodyLen - offset);
                memcpy(&bodyWord.data, body + offset, len);
                bodyWord.keep = (len == (WIDTH / 8)) ? -1 : (1 << len) - 1;
                bodyWord.last = (i == (bodyLen / (WIDTH / 8)));
                txDataBuffer.write(bodyWord);
            }
            clientFsmState = CHECK_TIME;
            break;
        }
        case CHECK_TIME:
            if (stopSend && closeIt == numConnections) {
                clientFsmState = IDLE;
            } else {
                if (stopSend) {
                    closeConnection.write(currentSessionID);
                    closeIt++;
                }

                if (closeIt != numConnections) {
                    clientFsmState = WAIT_CON;
                }
            }
            break;
    }

    // Clock handling
    if (!stopSignal.empty()) {
        stopSignal.read(timeOver);
    }
}


template <int WIDTH>
void server(	hls::stream<ap_uint<16> >&		listenPort,
				hls::stream<bool>&				listenPortStatus,
				hls::stream<appNotification>&	notifications,
				hls::stream<appReadRequest>&	readRequest,
				hls::stream<ap_uint<16> >&		rxMetaData,
				hls::stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >& rxData,
				hls::stream<http::http_request_spt>& http_request,
				hls::stream<http::pkt512>& http_request_headers,
				hls::stream<http::pkt512>& http_request_body)	
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS DATAFLOW

	http::listen_port(
		listenPort, 
		listenPortStatus, 
		80);
	
	http::request_processor(
		notifications, 
		readRequest, 
		rxMetaData, 
		rxData, 
		http_request, 
		http_request_headers, 
		http_request_body);

}

void clock( hls::stream<bool>&	startSignal,
            hls::stream<bool>&	stopSignal,
            ap_uint<64>          timeInCycles)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

   enum swStateType {WAIT_START, RUN};
   static swStateType sw_state = WAIT_START;
   static ap_uint<48> time = 0;

   switch (sw_state)
   {
   case WAIT_START:
      if (!startSignal.empty())
      {
         startSignal.read();
         time = 0;
         sw_state = RUN;
      }
      break;
   case RUN:
      time++;
      if (time == timeInCycles)
      {
         stopSignal.write(true);
         sw_state = WAIT_START;
      }
      break;
   }
}

void http_client(	hls::stream<ap_uint<16> >& listenPort,
					hls::stream<bool>& listenPortStatus,
					hls::stream<appNotification>& notifications,
					hls::stream<appReadRequest>& readRequest,
					hls::stream<ap_uint<16> >& rxMetaData,
					hls::stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >& rxData,
					hls::stream<ipTuple>& openConnection, 				// NOT USED
					hls::stream<openStatus>& openConStatus, 			// NOT USED 
					hls::stream<ap_uint<16> >& closeConnection,         // NOT USED
					hls::stream<appTxMeta>& txMetaData,                 // NOT USED
					hls::stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >& txData, // NOT USED
					hls::stream<appTxRsp>& txStatus,                    // NOT USED
					ap_uint<1>		runExperiment,
					ap_uint<1>		dualModeEn,
					ap_uint<14>		useConn,
					ap_uint<8>		pkgWordCount,
					ap_uint<8>		packetGap,
	               	ap_uint<32>    	timeInSeconds,
	               	ap_uint<64>    	timeInCycles,
	               	ap_uint<16>		useIpAddr,
	               	ap_uint<16>		regBasePort,
					ap_uint<32>		regIpAddress0,
					ap_uint<32>		regIpAddress1,
					ap_uint<32>		regIpAddress2,
					ap_uint<32>		regIpAddress3,
					ap_uint<32>		regIpAddress4,
					ap_uint<32>		regIpAddress5,
					ap_uint<32>		regIpAddress6,
					ap_uint<32>		regIpAddress7,
					ap_uint<32>		regIpAddress8,
					ap_uint<32>		regIpAddress9)

{
	#pragma HLS DATAFLOW disable_start_propagation
	#pragma HLS INTERFACE ap_ctrl_none port=return

	#pragma HLS INTERFACE axis register port=listenPort name=m_axis_listen_port
	#pragma HLS INTERFACE axis register port=listenPortStatus name=s_axis_listen_port_status

	#pragma HLS INTERFACE axis register port=notifications name=s_axis_notifications
	#pragma HLS INTERFACE axis register port=readRequest name=m_axis_read_package
	#pragma HLS aggregate compact=bit variable=notifications
	#pragma HLS aggregate compact=bit variable=readRequest

	#pragma HLS INTERFACE axis register port=rxMetaData name=s_axis_rx_metadata
	#pragma HLS INTERFACE axis register port=rxData name=s_axis_rx_data

	#pragma HLS INTERFACE axis register port=openConnection name=m_axis_open_connection
	#pragma HLS INTERFACE axis register port=openConStatus name=s_axis_open_status
	#pragma HLS aggregate compact=bit variable=openConnection
	#pragma HLS aggregate compact=bit variable=openConStatus

	#pragma HLS INTERFACE axis register port=closeConnection name=m_axis_close_connection

	#pragma HLS INTERFACE axis register port=txMetaData name=m_axis_tx_metadata
	#pragma HLS INTERFACE axis register port=txData name=m_axis_tx_data
	#pragma HLS INTERFACE axis register port=txStatus name=s_axis_tx_status
	#pragma HLS aggregate compact=bit variable=txMetaData
	#pragma HLS aggregate compact=bit variable=txStatus

	#pragma HLS INTERFACE ap_none register port=runExperiment
	#pragma HLS INTERFACE ap_none register port=dualModeEn
	#pragma HLS INTERFACE ap_none register port=useConn
	#pragma HLS INTERFACE ap_none register port=pkgWordCount
	#pragma HLS INTERFACE ap_none register port=packetGap
	#pragma HLS INTERFACE ap_none register port=regBasePort
	#pragma HLS INTERFACE ap_none register port=timeInSeconds
	#pragma HLS INTERFACE ap_none register port=timeInCycles
	#pragma HLS INTERFACE ap_none register port=regIpAddress0
	#pragma HLS INTERFACE ap_none register port=regIpAddress1
	#pragma HLS INTERFACE ap_none register port=regIpAddress2
	#pragma HLS INTERFACE ap_none register port=regIpAddress3
	#pragma HLS INTERFACE ap_none register port=regIpAddress4
	#pragma HLS INTERFACE ap_none register port=regIpAddress5
	#pragma HLS INTERFACE ap_none register port=regIpAddress6
	#pragma HLS INTERFACE ap_none register port=regIpAddress7
	#pragma HLS INTERFACE ap_none register port=regIpAddress8
	#pragma HLS INTERFACE ap_none register port=regIpAddress9
	#pragma HLS INTERFACE ap_none register port=useIpAddr

	static hls::stream<bool>		startSignalFifo("startSignalFifo");
	static hls::stream<bool>		stopSignalFifo("stopSignalFifo");
	#pragma HLS STREAM variable=startSignalFifo depth=2
	#pragma HLS STREAM variable=stopSignalFifo depth=2

	//This is required to buffer up to 1024 reponses => supporting up to 1024 connections
	static hls::stream<internalAppTxRsp>	txStatusBuffer("txStatusBuffer");
	#pragma HLS STREAM variable=txStatusBuffer depth=512

	//This is required to buffer up to 512 reponses => supporting up to 512 connections
	static hls::stream<openStatus>	openConStatusBuffer("openConStatusBuffer");
	#pragma HLS STREAM variable=openConStatusBuffer depth=512

	//This is required to buffer up to 512 tx_meta_data => supporting up to 512 connections
	static hls::stream<appTxMeta>	txMetaDataBuffer("txMetaDataBuffer");
	#pragma HLS STREAM variable=txMetaDataBuffer depth=512

	//This is required to buffer up to MAX_SESSIONS txData 
	static hls::stream<net_axis<DATA_WIDTH> >	txDataBuffer("txDataBuffer");
	#pragma HLS STREAM variable=txDataBuffer depth=512

	//This is required to buffer up to MAX_SESSIONS txData 
	static hls::stream<net_axis<DATA_WIDTH> >	rxDataBuffer("rxDataBuffer");
	#pragma HLS STREAM variable=rxDataBuffer depth=512

	hls::stream<http::http_request_spt> http_request("http_request");
  	hls::stream<http::pkt512> http_request_headers("http_request_headers");
  	hls::stream<http::pkt512> http_request_body("http_request_body");

	status_handler(txStatus, txStatusBuffer);
	openStatus_handler(openConStatus, openConStatusBuffer);
	txMetaData_handler(txMetaDataBuffer, txMetaData);
	txDataBuffer_handler<DATA_WIDTH>(txDataBuffer, txData);
	rxDataBuffer_handler<DATA_WIDTH>(rxData, rxDataBuffer);

	client<DATA_WIDTH>(	openConnection,
						openConStatusBuffer,
						closeConnection,
						txMetaDataBuffer,
						txDataBuffer,
						txStatusBuffer,
						startSignalFifo,
						stopSignalFifo,
						runExperiment,
						dualModeEn,
						useConn,
						useIpAddr,
						pkgWordCount,
						packetGap,
						timeInSeconds,
						regBasePort,
						regIpAddress0,
						regIpAddress1,
						regIpAddress2,
						regIpAddress3,
						regIpAddress4,
						regIpAddress5,
						regIpAddress6,
						regIpAddress7,
						regIpAddress8,
						regIpAddress9);
	
	server<DATA_WIDTH>(	listenPort,
			listenPortStatus,
			notifications,
			readRequest,
			rxMetaData,
			rxData,
			http_request,
			http_request_headers,
			http_request_body);

	// Tie off unused ports
	// tie_off_tcp_open_connection(openConnection, openConStatus);
	// tie_off_tcp_close_connection(closeConnection);
	// tie_off_tcp_tx(txMetaData, txData, txStatus);
	
	/*
	 * Clock
	 */
	clock(startSignalFifo,
			stopSignalFifo,
			timeInCycles);
}
