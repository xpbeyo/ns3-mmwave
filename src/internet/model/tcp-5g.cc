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
      .SetParent<TcpCongestionOps>()
      .SetGroupName("Internet")
      .AddConstructor<Tcp5G>()
      ;
    return tid;
  }

  Tcp5G::Tcp5G(void) : TcpCongestionOps()
  {
    NS_LOG_FUNCTION(this);
  }

  Tcp5G::Tcp5G(const Tcp5G& sock)
    : TcpCongestionOps(sock)
  {
    NS_LOG_FUNCTION(this);
  }

  Tcp5G::~Tcp5G(void)
  {
  }

  /**
   * \brief Tcp 5G slow start algorithm
   *
   * Defined in RFC 5681 as
   *
   * > During slow start, a TCP increments cwnd by at most SMSS bytes for
   * > each ACK received that cumulatively acknowledges new data.  Slow
   * > start ends when cwnd exceeds ssthresh (or, optionally, when it
   * > reaches it, as noted above) or when congestion is observed.  While
   * > traditionally TCP implementations have increased cwnd by precisely
   * > SMSS bytes upon receipt of an ACK covering new data, we RECOMMEND
   * > that TCP implementations increase cwnd, per:
   * >
   * >    cwnd += min (N, SMSS)                      (2)
   * >
   * > where N is the number of previously unacknowledged bytes acknowledged
   * > in the incoming ACK.
   *
   * The ns-3 implementation respect the RFC definition. Linux does something
   * different:
   * \verbatim
  u32 tcp_slow_start(struct tcp_sock *tp, u32 acked)
    {
      u32 cwnd = tp->snd_cwnd + acked;

      if (cwnd > tp->snd_ssthresh)
        cwnd = tp->snd_ssthresh + 1;
      acked -= cwnd - tp->snd_cwnd;
      tp->snd_cwnd = min(cwnd, tp->snd_cwnd_clamp);

      return acked;
    }
    \endverbatim
   *
   * As stated, we want to avoid the case when a cumulative ACK increases cWnd more
   * than a segment size, but we keep count of how many segments we have ignored,
   * and return them.
   *
   * \param tcb internal congestion state
   * \param segmentsAcked count of segments acked
   * \return the number of segments not considered for increasing the cWnd
   */
  uint32_t
    Tcp5G::SlowStart(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
  {
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (segmentsAcked >= 1)
    {
      tcb->m_cWnd += tcb->m_segmentSize;
      NS_LOG_INFO("In SlowStart, updated to cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);

      NS_LOG_DEBUG(
        "Time " <<
        Simulator::Now() <<
        ",SlowStart" <<
        ",target_ratio " <<
        target_ratio <<
        ",slow_start " <<
        true
      );
      return segmentsAcked - 1;
    }

    return 0;
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
    if (safe_zone >= 0)
    {
      if (delay_derivative < 0) // second part is the last change
      {
        cons = pow(1.5, progressive_coef);

        // target congestion window
        target_ratio = floor(cons * tcb->m_cWnd.Get());
        target_ratio = (alpha * target_ratio) + (1 - alpha) * m_cwndMean;

        NS_LOG_DEBUG("Progressive Gaussian module activted: " << cons);
        tcb->m_cWnd = std::max(std::min(target_ratio, MAX_CWND), MIN_CWND);
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
          tcb->m_cWnd = std::max(std::min(target_ratio, MAX_CWND), MIN_CWND);
        }
        else
        {
          target_ratio = MIN_CWND;
          NS_LOG_DEBUG("Very bad future cat module activted");
          tcb->m_cWnd = std::min(target_ratio, MAX_CWND);
        }
      }
    }

    else if (safe_zone > -1)
    {
      cons = pow(2, safe_zone - 1);
      target_ratio = floor(cons * tcb->m_cWnd);
      target_ratio = (alpha * target_ratio) + (1 - alpha) * m_cwndMean;

      NS_LOG_DEBUG("Safe zone module activted: " << cons);
      tcb->m_cWnd = std::max(std::min(target_ratio, MAX_CWND), MIN_CWND);
    }
    else
    {
      target_ratio = MIN_CWND;
      NS_LOG_DEBUG("Very bad safe zone module activted");
      tcb->m_cWnd = target_ratio;
      if (safe_zone < -8)
      {
        // TODO: Set tcp_cwnd_cap to target_ratio
      }
    }

    NS_LOG_DEBUG(
      "Time " <<
      Simulator::Now() <<
      "CongestionAvoidance Completed" <<
      ",delay_derivative " <<
      delay_derivative <<
      ",current_delay " <<
      current_delay <<
      ",min_rtt " <<
      min_rtt <<
      ",bad_delay " <<
      bad_delay <<
      ",safe_zone " <<
      safe_zone
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

    if (m_updateType == PER_PACKET_UPDATE)
    {
      UpdateState(tcb);
    }

    if (tcb->m_cWnd < tcb->m_ssThresh)
    {
      segmentsAcked = SlowStart(tcb, segmentsAcked);
    }

    if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
      CongestionAvoidance(tcb, segmentsAcked);
    }

    NS_LOG_DEBUG(
      "Time " <<
      Simulator::Now() <<
      ",IncreaseWindow Completed" <<
      ",cwnd " <<
      tcb->m_cWnd
    );

    /* At this point, we could have segmentsAcked != 0. This because RFC says
     * that in slow start, we should increase cWnd by min (N, SMSS); if in
     * slow start we receive a cumulative ACK, it counts only for 1 SMSS of
     * increase, wasting the others.
     *
     * // Incorrect assert, I am sorry
     * NS_ASSERT (segmentsAcked == 0);
     */
  }

  std::string
    Tcp5G::GetName() const
  {
    return "Tcp5G";
  }

  uint32_t
    Tcp5G::GetSsThresh(Ptr<const TcpSocketState> state,
      uint32_t bytesInFlight)
  {
    NS_LOG_FUNCTION(this << state << bytesInFlight);

    return std::max(2 * state->m_segmentSize, bytesInFlight / 2);
  }

  void
    Tcp5G::ReduceCwnd(Ptr<TcpSocketState> tcb)
  {
    NS_LOG_FUNCTION(this << tcb);

    tcb->m_cWnd = std::max(tcb->m_cWnd.Get() / 2, tcb->m_segmentSize);
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
        if (!m_logStarted)
        {
          LogState(tcb);
          m_logStarted = true;
        }
      }
    }
  }

  void
    Tcp5G::PeriodicUpdate(Ptr<TcpSocketState> tcb)
  {
    current_delay = tcb->m_lastRtt.Get();
    delay_var = tcb->m_lastRttVar.Get();
    min_rtt = tcb->m_minRtt;

    // check for mili, micro and etc
    if (rtt_threshold_factor != 0) {
      bad_delay = rtt_threshold_factor * min_rtt;
    }
    delay_derivative = ((current_delay - prev_delay) / report_period).GetDouble();
    if (abs(delay_derivative) < 0.02)
    {
      delay_derivative = 0;
    }
    safe_zone = 1 - ((current_delay - min_rtt) / bad_delay).GetDouble();
    predicted_queueing_delay = delay_derivative * report_period;
    if ((predicted_queueing_delay + current_delay - min_rtt) > bad_delay) {
      future_cat = true;
    }
    else {
      future_cat = false;
    }
    future_safezone = 1 - ((predicted_queueing_delay + current_delay - min_rtt) / bad_delay).GetDouble();
    prev_delay = current_delay;

    Simulator::Schedule(report_period, &Tcp5G::PeriodicUpdate, this, tcb);
  }

  void
    Tcp5G::UpdateState(Ptr<TcpSocketState> tcb)
  {
    if (!m_updateStarted)
    {
      m_lastUpdateTime = Simulator::Now();
      m_updateStarted = false;
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
  }

  void
    Tcp5G::LogState (Ptr<TcpSocketState> tcb)
  {
    NS_LOG_DEBUG(
      "Time " <<
      Simulator::Now() <<
      ",Update Completed" <<
      ",current_delay " <<
      current_delay <<
      ",delay_derivatve " <<
      delay_derivative <<
      ",cwnd " <<
      tcb->m_cWnd <<
      ",ssThresh " <<
      tcb->m_ssThresh
    );
    Simulator::Schedule(MilliSeconds(1), &Tcp5G::LogState, this, tcb);
  }
} // namespace ns3
