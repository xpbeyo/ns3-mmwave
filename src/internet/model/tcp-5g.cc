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
#include "tcp-5g.h"
#include "ns3/log.h"

namespace ns3 {
  // 5G
  NS_LOG_COMPONENT_DEFINE("Tcp5G");
  NS_OBJECT_ENSURE_REGISTERED(Tcp5G);

  TypeId
    Tcp5G::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::Tcp5G")
      .SetParent<TcpCubic>()
      .SetGroupName("Internet")
      .AddConstructor<Tcp5G>()
      ;
    return tid;
  }

  Tcp5G::Tcp5G(void) : TcpCubic()
  {
    NS_LOG_FUNCTION(this);
  }

  Tcp5G::Tcp5G(const Tcp5G& sock)
    : TcpCubic(sock)
  {
    NS_LOG_FUNCTION(this);
  }

  Tcp5G::~Tcp5G(void)
  {
  }

  /**
   * \brief 5G congestion avoidance
   *
   * During congestion avoidance, cwnd is incremented by roughly 1 full-sized
   * segment per round-trip time (RTT).
   *
   * \param tcb internal congestion state
   * \param segmentsAcked count of segments acked
   */
  void
    Tcp5G::CongestionAvoidance(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
  {
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    // Defintion of some constants
    int MAX_CWND = 10000;
    int MIN_CWND = 4;
    double alpha = 0.7;

    m_delayMean = m_delayMean - delay_derivative;
    Ptr<NormalRandomVariable> normal_distribution = CreateObject<NormalRandomVariable>();
    normal_distribution->SetAttribute("Mean", DoubleValue(m_delayMean));
    normal_distribution->SetAttribute("Variance", DoubleValue(abs(m_delayMean / (double)4)));

    double nrm_out = normal_distribution->GetValue();
    double progressive_coef = (1 / (double)(1 + exp(-1 * nrm_out)));

    double cons;
    int target_ratio = 150;

    if (safe_zone >= 0)
    {
      if (delay_derivative < 0) // second part is the last change
      {
        cons = pow(1.5, progressive_coef);

        // target congestion window
        target_ratio = floor(cons * tcb->m_cWnd.Get());
        target_ratio = (alpha * target_ratio) + (1 - alpha) * m_cwndMean;

        NS_LOG_DEBUG("Progressive Gaussian module activted: " << cons);
        tcb->m_cWnd = std::max(std::min(target_ratio, MAX_CWND), MIN_CWND) * tcb->m_segmentSize;
      }
      else if (future_cat)
      {
        // if predicted delay is double the bad delay
        if (future_safezone > -1)
        {
          cons = pow(2, future_safezone);
          target_ratio = floor(cons * tcb->m_cWnd.Get());
          target_ratio = (alpha * target_ratio) + (1 - alpha) * m_cwndMean;

          NS_LOG_DEBUG("Future cat module activted: " << cons);
          tcb->m_cWnd = std::max(std::min(target_ratio, MAX_CWND), MIN_CWND) * tcb->m_segmentSize;
        }
        else
        {
          target_ratio = MIN_CWND;
          NS_LOG_DEBUG("Very bad future cat module activted");
          tcb->m_cWnd = std::min(target_ratio, MAX_CWND) * tcb->m_segmentSize;
        }
      }
    }

    else if (safe_zone > -1)
    {
      cons = pow(2, safe_zone - 1);
      target_ratio = floor(cons * tcb->m_cWnd);
      target_ratio = (alpha * target_ratio) + (1 - alpha) * m_cwndMean;

      NS_LOG_DEBUG("Safe zone module activted: " << cons);
      tcb->m_cWnd = std::max(std::min(target_ratio, MAX_CWND), MIN_CWND) * tcb->m_segmentSize;
    }
    else
    {
      target_ratio = MIN_CWND;
      NS_LOG_DEBUG("Very bad safe zone module activted");
      tcb->m_cWnd = target_ratio * tcb->m_segmentSize;
      if (safe_zone < -8)
      {
        // TODO: Set tcp_cwnd_cap to target_ratio
      }
    }

    NS_LOG_INFO(
      "Time " <<
      Simulator::Now() <<
      ",CongestionAvoidance Completed" <<
      ",m_cWnd " <<
      tcb->m_cWnd
    );

    m_cwndCount = m_cwndCount + 1;
    m_cwndSum = m_cwndSum + target_ratio;
    m_cwndMean = m_cwndSum / (double)m_cwndCount;
  }

  /**
   * \brief Try to increase the cWnd following the 5G specification
   *
   * \see SlowStart
   * \see CongestionAvoidance
   *
   * \param tcb internal congestion state
   * \param segmentsAcked count of segments acked
   */
  void
    Tcp5G::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
  {
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    TcpCubic::IncreaseWindow(tcb, segmentsAcked);

    if (tcb->m_cWnd >= tcb->m_ssThresh && segmentsAcked > 0)
      {
        CongestionAvoidance(tcb, segmentsAcked);
      }

    NS_LOG_INFO(
      "Time " <<
      Simulator::Now() <<
      ",IncreaseWindow Completed" <<
      ",m_cWnd " <<
      tcb->m_cWnd
    );
  }

  void
  Tcp5G::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                      const Time &rtt)
  {
    NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);
    TcpCubic::PktsAcked(tcb, segmentsAcked, rtt);
    if (m_updateType == PER_PACKET_UPDATE)
    {
      UpdateState(tcb);
    }
  }

  std::string
    Tcp5G::GetName() const
  {
    return "Tcp5G";
  }

  Ptr<TcpCongestionOps>
    Tcp5G::Fork()
  {
    return CopyObject<Tcp5G>(this);
  }

  void
    Tcp5G::CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
  {
    NS_LOG_FUNCTION(this << tcb << event);
    if (m_updateType == PERIODIC_UPDATE)
    {
      if (event == TcpSocketState::CA_EVENT_TX_START && !m_periodicUpdateStarted)
      {
        NS_LOG_DEBUG("PeriodicUpdate Started");
        PeriodicUpdate(tcb);
        m_periodicUpdateStarted = true;
      }
    }
  }

  void
    Tcp5G::PeriodicUpdate(Ptr<TcpSocketState> tcb)
  {
    UpdateState(tcb);

    if (tcb->m_cWnd >= tcb->m_ssThresh)
      {
        CongestionAvoidance(tcb, 0);
      }

    Simulator::Schedule(min_rtt, &Tcp5G::PeriodicUpdate, this, tcb);
  }

  void
    Tcp5G::UpdateState(Ptr<TcpSocketState> tcb)
  {
    if (!m_updateStarted)
    {
      m_lastUpdateTime = Simulator::Now();
      m_updateStarted = true;
    }
    else
    {
      Time now = Simulator::Now();
      current_delay = tcb->m_lastRtt.Get();
      delay_var = tcb->m_lastRttVar.Get();
      min_rtt = tcb->m_minRtt;

      // check for mili, micro and etc
      if (rtt_threshold_factor != 0) {
        bad_delay = rtt_threshold_factor * min_rtt;
      }
      delay_derivative = ((current_delay - prev_delay) / (now - m_lastUpdateTime)).GetDouble();
      if (abs(delay_derivative) < 0.02)
      {
        delay_derivative = 0;
      }
      safe_zone = 1 - ((current_delay - min_rtt) / bad_delay).GetDouble();
      predicted_queueing_delay = delay_derivative * (now - m_lastUpdateTime);
      if ((predicted_queueing_delay + current_delay - min_rtt) > bad_delay) {
        future_cat = true;
      }
      else {
        future_cat = false;
      }
      future_safezone = 1 - ((predicted_queueing_delay + current_delay - min_rtt) / bad_delay).GetDouble();
      prev_delay = current_delay;
      m_lastUpdateTime = now;
    }

    NS_LOG_INFO(
      "Time " <<
      Simulator::Now() <<
      ",Update Completed" <<
      ",current_delay " <<
      current_delay <<
      ",delay_derivatve " <<
      delay_derivative <<
      ",m_cWnd " <<
      tcb->m_cWnd <<
      ",ssThresh " <<
      tcb->m_ssThresh
    );
  }
} // namespace ns3
