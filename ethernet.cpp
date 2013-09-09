/*
 * ethernet.cpp
 *
 *  Created on: Aug 8, 2013
 *      Author: caleb
 */

#include "ethernet.h"
#include "rpc_error.h"

Ethernet::Ethernet(){
    interface += "eth0";
    is_open = false;
}
Ethernet::Ethernet(std::string interface) {
    this->interface += interface;
    is_open = false;
}

Ethernet::~Ethernet(){}

void Ethernet::Write(const void *buffer, unsigned int size){
    if(!is_open) init_connection();
    for(unsigned int i = 0 ; i < size; i++){
        if(tx_payload_size == MAX_TX_DATA){
            Flush();
        }
        tx_frame[ETH_DATA_OFFSET + tx_payload_size] = ((char*)buffer)[i];
        tx_payload_size++;
    }
}
void Ethernet::Flush(){
    if(!is_open) init_connection();
    tx_frame[12] = tx_payload_size >> 8;
    tx_frame[13] = tx_payload_size;
    pcap_sendpacket(descr, tx_frame, tx_payload_size + ETH_DATA_OFFSET);
    tx_payload_size = 0;
}
void Ethernet::Clear(){
    if(!is_open) init_connection();
    tx_payload_size = 0;
    rx_buffer.clear();
}
void Ethernet::Read(void *buffer, unsigned int size){
    if(!is_open) init_connection();
    int timeout = 1000;
    for(unsigned int i = 0; i < size; i++){
        if(!rx_buffer.empty()){
            ((unsigned char*)buffer)[i] = rx_buffer.front();
            rx_buffer.pop_front();
        } else{
            const unsigned char* rx_frame = pcap_next(descr, &header);
            if(rx_frame == NULL){
                timeout--;
                i--;
                if(timeout == 0){
                    printf("Error reading from ethernet.\n");
                    throw CRpcError(CRpcError::TIMEOUT);
                }
            }
            if(header.len < 14){ // malformed message
                i--;
                continue;
            }
            
            unsigned int rx_payload_size = rx_frame[12];
            rx_payload_size = (rx_payload_size << 8) | rx_frame[13];
            if(rx_payload_size > 1500) {
                i--;
                continue; // message not from test board
            }

            bool mac_match = true;
            for(int j = 0; j < 6; j++){
                if(rx_frame[6 + j] != src_addr[j]){
                    mac_match = false;
                    break;
                } 
            }
            if(mac_match){ // message sent from host
                i--;
                continue;
            }

            printf("Received data of length (%d,%d): \n",rx_payload_size, header.len - 14);
            for(unsigned int j =0; j < rx_payload_size;j++){
                printf("%02X:",rx_frame[j + ETH_DATA_OFFSET]);
                rx_buffer.push_back(rx_frame[j + ETH_DATA_OFFSET]);
            }
            printf("\n\n");
            i--;
        }
    }
}


void Ethernet::Close(){
    if(is_open)    pcap_close(descr);
}

void Ethernet::init_connection(){
    rx_buffer.resize(0);
    for(int i =0; i < TX_FRAME_SIZE; i++){
        tx_frame[i] = 0;
    }
    tx_payload_size = 0;
    
    char errbuf[PCAP_ERRBUF_SIZE];
    descr = pcap_open_live(interface.c_str(), BUFSIZ,0,100,errbuf);
    if(descr == NULL){
        throw CRpcError(CRpcError::ETH_ERROR);
    }

    is_open = true;
    
    init_tx_frame();
}

void Ethernet::init_tx_frame(){
    //TODO: find MACs dynamically
    dst_addr[0] = 0xFF;
    dst_addr[1] = 0xFF;
    dst_addr[2] = 0xFF;
    dst_addr[3] = 0xFF;
    dst_addr[4] = 0xFF;
    dst_addr[5] = 0xFF;
    
    src_addr[0] = 0x00;
    src_addr[1] = 0x90;
    src_addr[2] = 0xf5;
    src_addr[3] = 0xc3;
    src_addr[4] = 0xba;
    src_addr[5] = 0x7f;
    for(int i = 0; i < 6; i++){
        tx_frame[i] = dst_addr[i];
        tx_frame[i+6] = src_addr[i];
    }
}

