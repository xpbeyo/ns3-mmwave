/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Natale Patriciello <natale.patriciello@gmail.com>
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

#define NS_LOG_APPEND_CONTEXT \
  { std::clog << Simulator::Now ().GetSeconds () << " "; }

#include "tcp-5g.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("Tcp5G");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (Tcp5G);

TypeId
Tcp5G::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Tcp5G")
    .SetParent<TcpSocketBase> ()
    .AddConstructor<Tcp5G> ()
    .SetGroupName ("Internet")
    .AddAttribute ("FastConvergence", "Enable (true) or disable (false) fast convergence",
                   BooleanValue (true),
                   MakeBooleanAccessor (&Tcp5G::m_fastConvergence),
                   MakeBooleanChecker ())
    .AddAttribute ("Beta", "Beta for multiplicative decrease",
                   DoubleValue (0.7),
                   MakeDoubleAccessor (&Tcp5G::m_beta),
                   MakeDoubleChecker <double> (0.0))
    .AddAttribute ("HyStart", "Enable (true) or disable (false) hybrid slow start algorithm",
                   BooleanValue (true),
                   MakeBooleanAccessor (&Tcp5G::m_hystart),
                   MakeBooleanChecker ())
    .AddAttribute ("HyStartLowWindow", "Lower bound cWnd for hybrid slow start (segments)",
                   UintegerValue (16),
                   MakeUintegerAccessor (&Tcp5G::m_hystartLowWindow),
                   MakeUintegerChecker <uint32_t> ())
    .AddAttribute ("HyStartDetect", "Hybrid Slow Start detection mechanisms:" \
                   "1: packet train, 2: delay, 3: both",
                   IntegerValue (3),
                   MakeIntegerAccessor (&Tcp5G::m_hystartDetect),
                   MakeIntegerChecker <int> (1,3))
    .AddAttribute ("HyStartMinSamples", "Number of delay samples for detecting the increase of delay",
                   UintegerValue (8),
                   MakeUintegerAccessor (&Tcp5G::m_hystartMinSamples),
                   MakeUintegerChecker <uint8_t> ())
    .AddAttribute ("HyStartAckDelta", "Spacing between ack's indicating train",
                   TimeValue (MilliSeconds (2)),
                   MakeTimeAccessor (&Tcp5G::m_hystartAckDelta),
                   MakeTimeChecker ())
    .AddAttribute ("HyStartDelayMin", "Minimum time for hystart algorithm",
                   TimeValue (MilliSeconds (4)),
                   MakeTimeAccessor (&Tcp5G::m_hystartDelayMin),
                   MakeTimeChecker ())
    .AddAttribute ("HyStartDelayMax", "Maximum time for hystart algorithm",
                   TimeValue (MilliSeconds (1000)),
                   MakeTimeAccessor (&Tcp5G::m_hystartDelayMax),
                   MakeTimeChecker ())
    .AddAttribute ("CubicDelta", "Delta Time to wait after fast recovery before adjusting param",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&Tcp5G::m_cubicDelta),
                   MakeTimeChecker ())
    .AddAttribute ("CntClamp", "Counter value when no losses are detected (counter is used" \
                   " when incrementing cWnd in congestion avoidance, to avoid" \
                   " floating point arithmetic). It is the modulo of the (avoided)" \
                   " division",
                   UintegerValue (20),
                   MakeUintegerAccessor (&Tcp5G::m_cntClamp),
                   MakeUintegerChecker <uint8_t> ())
    .AddAttribute ("C", "Cubic Scaling factor",
                   DoubleValue (0.4),
                   MakeDoubleAccessor (&Tcp5G::m_c),
                   MakeDoubleChecker <double> (0.0))
  ;
  return tid;
}

Tcp5G::Tcp5G ()
  : TcpCongestionOps (),
    m_cWndCnt (0),
    m_lastMaxCwnd (0),
    m_bicOriginPoint (0),
    m_bicK (0.0),
    m_delayMin (Time::Min ()),
    m_epochStart (Time::Min ()),
    m_found (false),
    m_roundStart (Time::Min ()),
    m_endSeq (0),
    m_lastAck (Time::Min ()),
    m_cubicDelta (Time::Min ()),
    m_currRtt (Time::Min ()),
    m_sampleCnt (0),
    m_factorMultiStarted (false)
{
  NS_LOG_FUNCTION (this);
}

Tcp5G::Tcp5G (const Tcp5G &sock)
  : TcpCongestionOps (sock),
    m_fastConvergence (sock.m_fastConvergence),
    m_beta (sock.m_beta),
    m_hystart (sock.m_hystart),
    m_hystartDetect (sock.m_hystartDetect),
    m_hystartLowWindow (sock.m_hystartLowWindow),
    m_hystartAckDelta (sock.m_hystartAckDelta),
    m_hystartDelayMin (sock.m_hystartDelayMin),
    m_hystartDelayMax (sock.m_hystartDelayMax),
    m_hystartMinSamples (sock.m_hystartMinSamples),
    m_initialCwnd (sock.m_initialCwnd),
    m_cntClamp (sock.m_cntClamp),
    m_c (sock.m_c),
    m_cWndCnt (sock.m_cWndCnt),
    m_lastMaxCwnd (sock.m_lastMaxCwnd),
    m_bicOriginPoint (sock.m_bicOriginPoint),
    m_bicK (sock.m_bicK),
    m_delayMin (sock.m_delayMin),
    m_epochStart (sock.m_epochStart),
    m_found (sock.m_found),
    m_roundStart (sock.m_roundStart),
    m_endSeq (sock.m_endSeq),
    m_lastAck (sock.m_lastAck),
    m_cubicDelta (sock.m_cubicDelta),
    m_currRtt (sock.m_currRtt),
    m_sampleCnt (sock.m_sampleCnt),
    m_factorMultiStarted (false)
{
  NS_LOG_FUNCTION (this);
}

std::string
Tcp5G::GetName () const
{
  return "Tcp5G";
}

void
Tcp5G::HystartReset (Ptr<const TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this);

  m_roundStart = m_lastAck = Simulator::Now ();
  m_endSeq = tcb->m_highTxMark;
  m_currRtt = Time::Min ();
  m_sampleCnt = 0;
}

void
Tcp5G::TimesCwnd(Ptr<TcpSocketState> tcb)
{
  tcb->m_cWnd *= 2;
  Simulator::Schedule(Time(MilliSeconds(10)), &Tcp5G::TimesCwnd, this, tcb);
}

void
Tcp5G::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (!m_factorMultiStarted)
  {
    TimesCwnd(tcb);
  }

  if (tcb->m_cWnd < tcb->m_ssThresh)
    {

      if (m_hystart && tcb->m_lastAckedSeq > m_endSeq)
        {
          HystartReset (tcb);
        }

      tcb->m_cWnd += tcb->m_segmentSize;
      segmentsAcked -= 1;

      NS_LOG_INFO ("In SlowStart, updated to cwnd " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    }

  if (tcb->m_cWnd >= tcb->m_ssThresh && segmentsAcked > 0)
    {
      m_cWndCnt += segmentsAcked;
      uint32_t cnt = Update (tcb);

      /* According to RFC 6356 even once the new cwnd is
       * calculated you must compare this to the number of ACKs received since
       * the last cwnd update. If not enough ACKs have been received then cwnd
       * cannot be updated.
       */
      if (m_cWndCnt >= cnt)
        {
          tcb->m_cWnd += tcb->m_segmentSize;
          m_cWndCnt -= cnt;
          NS_LOG_INFO ("In CongAvoid, updated to cwnd " << tcb->m_cWnd);
        }
      else
        {
          NS_LOG_INFO ("Not enough segments have been ACKed to increment cwnd."
                       "Until now " << m_cWndCnt << " cnd " << cnt);
        }
    }
}

uint32_t
Tcp5G::Update (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this);
  Time t;
  uint32_t delta, bicTarget, cnt = 0;
  double offs;
  uint32_t segCwnd = tcb->GetCwndInSegments ();

  if (m_epochStart == Time::Min ())
    {
      m_epochStart = Simulator::Now ();   // record the beginning of an epoch

      if (m_lastMaxCwnd <= segCwnd)
        {
          NS_LOG_DEBUG ("lastMaxCwnd <= m_cWnd. K=0 and origin=" << segCwnd);
          m_bicK = 0.0;
          m_bicOriginPoint = segCwnd;
        }
      else
        {
          m_bicK = std::pow ((m_lastMaxCwnd - segCwnd) / m_c, 1 / 3.);
          m_bicOriginPoint = m_lastMaxCwnd;
          NS_LOG_DEBUG ("lastMaxCwnd > m_cWnd. K=" << m_bicK <<
                        " and origin=" << m_lastMaxCwnd);
        }
    }

  t = Simulator::Now () + m_delayMin - m_epochStart;

  if (t.GetSeconds () < m_bicK)       /* t - K */
    {
      offs = m_bicK - t.GetSeconds ();
      NS_LOG_DEBUG ("t=" << t.GetSeconds () << " <k: offs=" << offs);
    }
  else
    {
      offs = t.GetSeconds () - m_bicK;
      NS_LOG_DEBUG ("t=" << t.GetSeconds () << " >= k: offs=" << offs);
    }


  /* Constant value taken from Experimental Evaluation of Cubic Tcp, available at
   * eprints.nuim.ie/1716/1/Hamiltonpfldnet2007_cubic_final.pdf */
  delta = m_c * std::pow (offs, 3);

  NS_LOG_DEBUG ("delta: " << delta);

  if (t.GetSeconds () < m_bicK)
    {
      // below origin
      bicTarget = m_bicOriginPoint - delta;
      NS_LOG_DEBUG ("t < k: Bic Target: " << bicTarget);
    }
  else
    {
      // above origin
      bicTarget = m_bicOriginPoint + delta;
      NS_LOG_DEBUG ("t >= k: Bic Target: " << bicTarget);
    }

  // Next the window target is converted into a cnt or count value. CUBIC will
  // wait until enough new ACKs have arrived that a counter meets or exceeds
  // this cnt value. This is how the CUBIC implementation simulates growing
  // cwnd by values other than 1 segment size.
  if (bicTarget > segCwnd)
    {
      cnt = segCwnd / (bicTarget - segCwnd);
      NS_LOG_DEBUG ("target>cwnd. cnt=" << cnt);
    }
  else
    {
      cnt = 100 * segCwnd;
    }

  if (m_lastMaxCwnd == 0 && cnt > m_cntClamp)
    {
      cnt = m_cntClamp;
    }

  // The maximum rate of cwnd increase CUBIC allows is 1 packet per
  // 2 packets ACKed, meaning cwnd grows at 1.5x per RTT.
  return std::max (cnt, 2U);
}

void
Tcp5G::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time &rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

  /* Discard delay samples right after fast recovery */
  if (m_epochStart != Time::Min ()
      && (Simulator::Now () - m_epochStart) < m_cubicDelta)
    {
      return;
    }

  /* first time call or link delay decreases */
  if (m_delayMin == Time::Min () || m_delayMin > rtt)
    {
      m_delayMin = rtt;
    }

  /* hystart triggers when cwnd is larger than some threshold */
  if (m_hystart
      && tcb->m_cWnd <= tcb->m_ssThresh
      && tcb->m_cWnd >= m_hystartLowWindow * tcb->m_segmentSize)
    {
      HystartUpdate (tcb, rtt);
    }
}

void
Tcp5G::HystartUpdate (Ptr<TcpSocketState> tcb, const Time& delay)
{
  NS_LOG_FUNCTION (this << delay);

  if (!(m_found & m_hystartDetect))
    {
      Time now = Simulator::Now ();

      /* first detection parameter - ack-train detection */
      if ((now - m_lastAck) <= m_hystartAckDelta)
        {
          m_lastAck = now;

          if ((now - m_roundStart) > m_delayMin)
            {
              m_found |= PACKET_TRAIN;
            }
        }

      /* obtain the minimum delay of more than sampling packets */
      if (m_sampleCnt < m_hystartMinSamples)
        {
          if (m_currRtt == Time::Min () || m_currRtt > delay)
            {
              m_currRtt = delay;
            }

          ++m_sampleCnt;
        }
      else
        {
          if (m_currRtt > m_delayMin +
              HystartDelayThresh (m_delayMin))
            {
              m_found |= DELAY;
            }
        }
      /*
       * Either one of two conditions are met,
       * we exit from slow start immediately.
       */
      if (m_found & m_hystartDetect)
        {
          NS_LOG_DEBUG ("Exit from SS, immediately :-)");
          tcb->m_ssThresh = tcb->m_cWnd;
        }
    }
}

Time
Tcp5G::HystartDelayThresh (const Time& t) const
{
  NS_LOG_FUNCTION (this << t);

  Time ret = t;
  if (t > m_hystartDelayMax)
    {
      ret = m_hystartDelayMax;
    }
  else if (t < m_hystartDelayMin)
    {
      ret = m_hystartDelayMin;
    }

  return ret;
}

uint32_t
Tcp5G::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);

  // Without inflation and deflation, these two are the same
  uint32_t segInFlight = bytesInFlight / tcb->m_segmentSize;
  uint32_t segCwnd = tcb->GetCwndInSegments ();
  NS_LOG_DEBUG ("Loss at cWnd=" << segCwnd << " in flight=" << segInFlight);

  /* Wmax and fast convergence */
  if (segCwnd < m_lastMaxCwnd && m_fastConvergence)
    {
      m_lastMaxCwnd = (segCwnd * (1 + m_beta)) / 2; // Section 4.6 in RFC 8312
    }
  else
    {
      m_lastMaxCwnd = segCwnd;
    }

  m_epochStart = Time::Min ();    // end of epoch

  /* Formula taken from the Linux kernel */
  uint32_t ssThresh = std::max (static_cast<uint32_t> (segInFlight * m_beta ), 2U) * tcb->m_segmentSize;

  NS_LOG_DEBUG ("SsThresh = " << ssThresh);

  return ssThresh;
}

void
Tcp5G::ReduceCwnd (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  tcb->m_cWnd = std::max (tcb->m_cWnd.Get () / 2, tcb->m_segmentSize);
}

void
Tcp5G::CongestionStateSet (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCongState_t newState)
{
  NS_LOG_FUNCTION (this << tcb << newState);

  if (newState == TcpSocketState::CA_LOSS)
    {
      CubicReset (tcb);
      HystartReset (tcb);
    }
}

void
Tcp5G::CubicReset (Ptr<const TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  m_lastMaxCwnd = 0;
  m_bicOriginPoint = 0;
  m_bicK = 0;
  m_delayMin = Time::Min ();
  m_found = false;
}

Ptr<TcpCongestionOps>
Tcp5G::Fork (void)
{
  NS_LOG_FUNCTION (this);
  return CopyObject<Tcp5G> (this);
}

}
