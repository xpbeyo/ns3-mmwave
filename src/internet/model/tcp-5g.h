/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Natale Patriciello <natale.patriciello@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef TCP5G_H
#define TCP5G_H

#include "ns3/tcp-cubic.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/random-variable-stream.h"
#include <algorithm>
#include "math.h"

namespace ns3 {
/**
 * \brief The 5G implementation
 */
class Tcp5G : public TcpCubic
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  Tcp5G ();

  /**
   * \brief Copy constructor.
   * \param sock object to copy.
   */
  Tcp5G (const Tcp5G& sock);

  ~Tcp5G ();

  std::string GetName () const;

  virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt);
  virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  virtual Ptr<TcpCongestionOps> Fork ();
  virtual void CwndEvent (Ptr<TcpSocketState> tcb,
                          const TcpSocketState::TcpCAEvent_t event);

  typedef enum {
    PERIODIC_UPDATE,
    PER_PACKET_UPDATE
  } update_type;

protected:
  virtual void CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);

  Time current_delay;
  Time delay_var;
  Time min_rtt;

  // Periodic update variables
  bool m_periodicUpdateStarted {false};
  Time report_period {MicroSeconds(20)};
  virtual void PeriodicUpdate(Ptr<TcpSocketState> tcb);

  // Per packet update
  bool m_updateStarted {false};
  Time m_lastUpdateTime;
  virtual void UpdateState(Ptr<TcpSocketState> tcb);

private:
  update_type m_updateType {PER_PACKET_UPDATE};
  float rtt_threshold_factor {0};
  Time bad_delay {MicroSeconds(20000)}; // In milliseconds
  double delay_derivative {0.0};
  Time prev_delay;
  Time predicted_queueing_delay;
  bool future_cat {false};
  double safe_zone {0.0};
  double future_safezone {0.0};

  double m_delayMean {0};
  int m_cwndSum {0};
  double m_cwndMean {0};
  int m_cwndCount;
};

} // namespace ns3

#endif // TCP5G_H
