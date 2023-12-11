/*
 *   Copyright (C) 2015,2016,2017,2018 by Jonathan Naylor G4KLX
 *   Copyright (C) 2015 by Jim Mclaughlin KI6ZUM
 *   Copyright (C) 2016 by Colin Durbridge G4EML
 *   Copyright (C) 2021,2022,2023 by Adrian Musceac YO8RZZ
 * 
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "Config.h"
#include "Globals.h"
#include "IO.h"
#include <pthread.h>
#include <vector>

#if defined(RPI)

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <chrono>


const uint16_t DC_OFFSET = 2048U;

unsigned char wavheader[] = {0x52,0x49,0x46,0x46,0xb8,0xc0,0x8f,0x00,0x57,0x41,0x56,0x45,0x66,0x6d,0x74,0x20,0x10,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0xc0,0x5d,0x00,0x00,0x80,0xbb,0x00,0x00,0x02,0x00,0x10,0x00,0x64,0x61,0x74,0x61,0xff,0xff,0xff,0xff};
std::chrono::high_resolution_clock::time_point tm1 = std::chrono::high_resolution_clock::now();
std::chrono::high_resolution_clock::time_point tm2 = std::chrono::high_resolution_clock::now();

void CIO::initInt()
{

//	std::cout << "IO Init" << std::endl;
	DEBUG1("IO Init done! Thread Started!");

}

void CIO::startInt()
{
    
	DEBUG1("IO Int start()");
    if (::pthread_mutex_init(&m_TXlock, NULL) != 0)
    {
        printf("\n Tx mutex init failed\n");
        exit(1);;
    }
    if (::pthread_mutex_init(&m_RXlock, NULL) != 0)
    {
        printf("\n RX mutex init failed\n");
        exit(1);;
    }

    ::pthread_create(&m_thread, NULL, helper, this);
    ::pthread_create(&m_threadRX, NULL, helperRX, this);
    ::pthread_setname_np(m_thread, "mmdvm_tx");
    ::pthread_setname_np(m_threadRX, "mmdvm_rx");
}

void* CIO::helper(void* arg)
{
  CIO* p = (CIO*)arg;

  while (1)
  {
    p->interrupt();
  }

  return NULL;
}

void* CIO::helperRX(void* arg)
{
  CIO* p = (CIO*)arg;

  while (1)
  {
    p->interruptRX();
  }

  return NULL;
}


void CIO::interrupt()
{

    TSample sample;
    uint32_t num_items = 720;
    zmq::message_t request_message;
    zmq::recv_result_t recv_result = m_zmqsocket.recv(request_message);
    if(request_message.size() < 1)
        return;
    ::pthread_mutex_lock(&m_TXlock);
    if(m_txBuffer.getData() >= num_items)
    {
        
        while(m_txBuffer.get(sample))
        {

            sample.sample *= 5;		// amplify by 12dB	
            m_samplebuf.push_back((int16_t)sample.sample);
            m_controlbuf.push_back((uint8_t)sample.control);

            if(m_samplebuf.size() >= num_items)
            {
                int buf_size = sizeof(uint32_t) + num_items * sizeof(uint8_t) + num_items * sizeof(int16_t);
                
                zmq::message_t reply (buf_size);
                memcpy (reply.data (), &num_items, sizeof(uint32_t));
                memcpy ((unsigned char *)reply.data () + sizeof(uint32_t), (unsigned char *)m_controlbuf.data(), num_items * sizeof(uint8_t));
                memcpy ((unsigned char *)reply.data () + sizeof(uint32_t) + num_items * sizeof(uint8_t),
                        (unsigned char *)m_samplebuf.data(), num_items*sizeof(int16_t));
                m_zmqsocket.send (reply, zmq::send_flags::dontwait);
                m_samplebuf.erase(m_samplebuf.begin(), m_samplebuf.begin()+num_items);
                m_controlbuf.erase(m_controlbuf.begin(), m_controlbuf.begin()+num_items);
            }
        }
        ::pthread_mutex_unlock(&m_TXlock);
    }
    else
    {
        ::pthread_mutex_unlock(&m_TXlock);
        zmq::message_t reply (sizeof(uint32_t));
        uint32_t items = 0;
        memcpy (reply.data (), &items, sizeof(uint32_t));
        m_zmqsocket.send (reply, zmq::send_flags::dontwait);
    }
       
   
#if defined(SEND_RSSI_DATA)
    //m_rssiBuffer.put(ADC->ADC_CDR[RSSI_CDR_Chan]);
#else
    //m_rssiBuffer.put(0U);
#endif

    //m_watchdog++;
	
}

void CIO::interruptRX()
{

    uint16_t sample = DC_OFFSET;
    uint8_t control = MARK_NONE;
    zmq::message_t mq_message;
    zmq::recv_result_t recv_result = m_zmqsocketRX.recv(mq_message, zmq::recv_flags::none);
    int size = mq_message.size();
    uint32_t data_size = 0;
    uint32_t rssi = 0;
    if(size < 1)
        return;
    memcpy(&data_size, (unsigned char*)mq_message.data(), sizeof(uint32_t));
    memcpy(&rssi, (unsigned char*)mq_message.data() + sizeof(uint32_t), sizeof(uint32_t));
    
    u_int16_t rx_buf_space = 0;
    while(rx_buf_space < data_size)
    {
        ::pthread_mutex_lock(&m_RXlock);
        rx_buf_space = m_rxBuffer.getSpace();
        ::pthread_mutex_unlock(&m_RXlock);
        if(rx_buf_space >= data_size)
            break;
        struct timespec local_time;
        clock_gettime(CLOCK_REALTIME, &local_time);

        local_time.tv_nsec += 20000;
        if(local_time.tv_nsec > 999999999)
        {
          local_time.tv_sec++;
          local_time.tv_nsec -= 1000000000;
        }
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &local_time, NULL);
    }
    ::pthread_mutex_lock(&m_RXlock);
    for(int i=0;i < data_size;i++)
    {
        int16_t signed_sample = 0;
        memcpy(&control, (unsigned char*)mq_message.data() + sizeof(uint32_t) + sizeof(uint32_t) + i, sizeof(uint8_t));
        memcpy(&signed_sample, (unsigned char*)mq_message.data() + sizeof(uint32_t) + sizeof(uint32_t) + data_size * sizeof(uint8_t) + i * sizeof(int16_t), sizeof(int16_t));
        m_rxBuffer.put({signed_sample, control});
        m_rssiBuffer.put((uint16_t) rssi);
    }
    ::pthread_mutex_unlock(&m_RXlock);
    return;
}

bool CIO::getCOSInt()
{
	return m_COSint;
}

void CIO::setLEDInt(bool on)
{
}

void CIO::setPTTInt(bool on)
{
	//handle enable clock gpio4

}

void CIO::setCOSInt(bool on)
{
    m_COSint = on;
}

void CIO::setDStarInt(bool on)
{
}

void CIO::setDMRInt(bool on)
{
}

void CIO::setYSFInt(bool on)
{
}

void CIO::setP25Int(bool on)
{
}

void CIO::setNXDNInt(bool on)
{
}

void CIO::delayInt(unsigned int dly)
{
  usleep(dly*1000);
}

uint8_t CIO::getCPU() const
{
    return 2;
}

void CIO::getUDID(uint8_t *reply_data)
{
    return;
}

#endif

