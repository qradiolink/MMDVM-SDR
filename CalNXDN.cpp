/*
 *   Copyright (C) 2018 by Andy Uribe CA6JAU
 *   Copyright (C) 2020 by Jonathan Naylor G4KLX
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

#if defined(MODE_NXDN)

#include "Globals.h"
#include "CalNXDN.h"

// NXDN 1031 Hz Test Pattern, RAN: 1, Unit ID: 1, Dst Group ID: 1, Outbound Direction
const uint8_t NXDN_CAL1K[4][49] = {
                            {0x00U,
                             0xCDU, 0xF5U, 0x9DU, 0x5DU, 0x7CU, 0xFAU, 0x0AU, 0x6EU, 0x8AU, 0x23U, 0x56U, 0xE8U,
                             0x4CU, 0xAAU, 0xDEU, 0x8BU, 0x26U, 0xE4U, 0xF2U, 0x82U, 0x88U,
                             0xC6U, 0x8AU, 0x74U, 0x29U, 0xA4U, 0xECU, 0xD0U, 0x08U, 0x22U,
                             0xCEU, 0xA2U, 0xFCU, 0x01U, 0x8CU, 0xECU, 0xDAU, 0x0AU, 0xA0U,
                             0xEEU, 0x8AU, 0x7EU, 0x2BU, 0x26U, 0xCCU, 0xF8U, 0x8AU, 0x08U},

                            {0x00U,
                             0xCDU, 0xF5U, 0x9DU, 0x5DU, 0x7CU, 0x6DU, 0xBBU, 0x0EU, 0xB3U, 0xA4U, 0x26U, 0xA8U,
                             0x4CU, 0xAAU, 0xDEU, 0x8BU, 0x26U, 0xE4U, 0xF2U, 0x82U, 0x88U,
                             0xC6U, 0x8AU, 0x74U, 0x29U, 0xA4U, 0xECU, 0xD0U, 0x08U, 0x22U,
                             0xCEU, 0xA2U, 0xFCU, 0x01U, 0x8CU, 0xECU, 0xDAU, 0x0AU, 0xA0U,
                             0xEEU, 0x8AU, 0x7EU, 0x2BU, 0x26U, 0xCCU, 0xF8U, 0x8AU, 0x08U},

                            {0x00U,
                             0xCDU, 0xF5U, 0x9DU, 0x5DU, 0x76U, 0x3AU, 0x1BU, 0x4AU, 0x81U, 0xA8U, 0xE2U, 0x80U,
                             0x4CU, 0xAAU, 0xDEU, 0x8BU, 0x26U, 0xE4U, 0xF2U, 0x82U, 0x88U,
                             0xC6U, 0x8AU, 0x74U, 0x29U, 0xA4U, 0xECU, 0xD0U, 0x08U, 0x22U,
                             0xCEU, 0xA2U, 0xFCU, 0x01U, 0x8CU, 0xECU, 0xDAU, 0x0AU, 0xA0U,
                             0xEEU, 0x8AU, 0x7EU, 0x2BU, 0x26U, 0xCCU, 0xF8U, 0x8AU, 0x08U},

                            {0x00U,
                             0xCDU, 0xF5U, 0x9DU, 0x5DU, 0x74U, 0x28U, 0x83U, 0x02U, 0xB0U, 0x2DU, 0x07U, 0xE2U,
                             0x4CU, 0xAAU, 0xDEU, 0x8BU, 0x26U, 0xE4U, 0xF2U, 0x82U, 0x88U,
                             0xC6U, 0x8AU, 0x74U, 0x29U, 0xA4U, 0xECU, 0xD0U, 0x08U, 0x22U,
                             0xCEU, 0xA2U, 0xFCU, 0x01U, 0x8CU, 0xECU, 0xDAU, 0x0AU, 0xA0U,
                             0xEEU, 0x8AU, 0x7EU, 0x2BU, 0x26U, 0xCCU, 0xF8U, 0x8AU, 0x08U}};

CCalNXDN::CCalNXDN() :
m_transmit(false),
m_state(NXDNCAL1K_IDLE),
m_audioSeq(0U)
{
}

void CCalNXDN::process()
{
  nxdnTX.process();

  uint16_t space = nxdnTX.getSpace();
  if (space < 1U)
    return;

  switch (m_state) {
    case NXDNCAL1K_TX:
      nxdnTX.writeData(NXDN_CAL1K[m_audioSeq], NXDN_FRAME_LENGTH_BYTES + 1U);
      m_audioSeq = (m_audioSeq + 1U) % 4U;
      if(!m_transmit)
        m_state = NXDNCAL1K_IDLE;
      break;
    default:
        m_state = NXDNCAL1K_IDLE;
        m_audioSeq = 0U;
      break;
  }
}

uint8_t CCalNXDN::write(const uint8_t* data, uint16_t length)
{
  if (length != 1U)
    return 4U;

  m_transmit = data[0U] == 1U;

  if(m_transmit && m_state == NXDNCAL1K_IDLE)
    m_state = NXDNCAL1K_TX;

  return 0U;
}

#endif

