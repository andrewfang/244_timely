/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 ResiliNets, ITTC, University of Kansas
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
 * Author: Truc Anh N. Nguyen <annguyen@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 */

#include "tcp-timely.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/log.h"
#include <sys/time.h>
#include <float.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpTimely");
NS_OBJECT_ENSURE_REGISTERED (TcpTimely);

TypeId
TcpTimely::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpTimely")
    .SetParent<TcpNewReno> ()
    .AddConstructor<TcpTimely> ()
    .SetGroupName ("Internet")
    .AddAttribute ("Alpha", "Lower bound of packets in network",
                   UintegerValue (2),
                   MakeUintegerAccessor (&TcpTimely::m_alpha),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Beta", "Upper bound of packets in network",
                   UintegerValue (4),
                   MakeUintegerAccessor (&TcpTimely::m_beta),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Gamma", "Limit on increase",
                   UintegerValue (1),
                   MakeUintegerAccessor (&TcpTimely::m_gamma),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

TcpTimely::TcpTimely (void)
  : TcpNewReno (),
    m_alpha (2),
    m_beta (4),
    m_gamma (1),
    m_baseRtt (Time::Max ()),
    m_minRtt (DBL_MAX),
    m_cntRtt (0),
    m_doingTimelyNow (true),
    m_begSndNxt (0),
    m_prevRtt(DBL_MAX),
    m_rttDiffMs(0),
    m_completionEvents(0)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO("TIMELY");
}

TcpTimely::TcpTimely (const TcpTimely& sock)
  : TcpNewReno (sock),
    m_alpha (sock.m_alpha),
    m_beta (sock.m_beta),
    m_gamma (sock.m_gamma),
    m_baseRtt (sock.m_baseRtt),
    m_minRtt (sock.m_minRtt),
    m_cntRtt (sock.m_cntRtt),
    m_doingTimelyNow (true),
    m_begSndNxt (0),
    m_prevRtt(sock.m_prevRtt),
    m_rttDiffMs(0),
    m_completionEvents(0)
{
  NS_LOG_FUNCTION (this);
}

TcpTimely::~TcpTimely (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<TcpCongestionOps>
TcpTimely::Fork (void)
{
  return CopyObject<TcpTimely> (this);
}

void
TcpTimely::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time& rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

  if (rtt.IsZero ())
    {
      return;
    }
  
  bool useOracle = false;
  double EWMA = 0.1;
  double BETA = 0.5;
  double ADDSTEP = 1500;
  double TLOW = 50;
  double THIGH = 500;

  uint32_t rate = tcb->m_cWnd;

  double measurement = useOracle ? 1 : rtt.GetMilliSeconds();

  m_minRtt = std::min (m_minRtt, measurement);

  double new_rtt_diff_ms = measurement - m_prevRtt;
  m_prevRtt = measurement;
  m_rttDiffMs = (1 - EWMA ) * m_rttDiffMs + EWMA * new_rtt_diff_ms;
  double normalized_gradient = m_rttDiffMs / m_minRtt;

  timeval time;
  gettimeofday(&time, NULL);
  long millis = (time.tv_sec * 1000) + (time.tv_usec / 1000);
  NS_LOG_INFO(rtt.GetMilliSeconds() << " " << millis);

  if (measurement < TLOW) {
    NS_LOG_INFO( "too low" );
    m_completionEvents = 0;
    rate = rate + ADDSTEP;
    tcb->m_cWnd = rate;
    NS_LOG_INFO("window size is now: " << tcb->m_cWnd);
    return;
  } else if (measurement > THIGH) {
    NS_LOG_INFO( "too high" );
    m_completionEvents = 0;
    rate = rate * (1 - BETA * (1 - THIGH/measurement));
    tcb->m_cWnd = rate;
    NS_LOG_INFO("window size is now: " << tcb->m_cWnd);
    return;
  }
  
  if (normalized_gradient <= 0) {
    NS_LOG_INFO( "normalized gradient" );
    m_completionEvents += 1;
    int N = 1;
    if (m_completionEvents == 5) {
      NS_LOG_INFO( "Entering HAI mode" );
      N = 5;
      //m_completionEvents = 0; // Not sure if need to reset to get out of HAI mode?
    }
    rate = rate + N * ADDSTEP;
  } else {
    rate = rate * (1 - BETA * normalized_gradient);
  }
  
  tcb->m_cWnd = rate;
  NS_LOG_INFO("window size is now: " << tcb->m_cWnd);


  m_baseRtt = std::min (m_baseRtt, rtt);
  NS_LOG_INFO ("Updated m_baseRtt = " << m_baseRtt);

  // Update RTT counter
  m_cntRtt++;
  NS_LOG_INFO ("Updated m_cntRtt = " << m_cntRtt);
}

void
TcpTimely::EnableTimely (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  m_doingTimelyNow = true;
  m_begSndNxt = tcb->m_nextTxSequence;
  m_cntRtt = 0;
  m_minRtt = DBL_MAX;
}

void
TcpTimely::DisableTimely ()
{
  NS_LOG_FUNCTION (this);

  m_doingTimelyNow = false;
}

void
TcpTimely::CongestionStateSet (Ptr<TcpSocketState> tcb,
                              const TcpSocketState::TcpCongState_t newState)
{
  NS_LOG_FUNCTION (this << tcb << newState);
  if (newState == TcpSocketState::CA_OPEN)
    {
      EnableTimely (tcb);
    }
  else
    {
      DisableTimely ();
    }
}

void
TcpTimely::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  return;
}

std::string
TcpTimely::GetName () const
{
  return "TIMELY";
}

uint32_t
TcpTimely::GetSsThresh (Ptr<const TcpSocketState> tcb,
                       uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);
  return std::max (std::min (tcb->m_ssThresh.Get (), tcb->m_cWnd.Get () - tcb->m_segmentSize), 2 * tcb->m_segmentSize);
}

} // namespace ns3
