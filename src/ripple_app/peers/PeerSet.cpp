//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

namespace ripple {

class InboundLedger;

// VFALCO NOTE The txnData constructor parameter is a code smell.
//             It is true if we are the base of a TransactionAcquire,
//             or false if we are base of InboundLedger. All it does
//             is change the behavior of the timer depending on the
//             derived class. Why not just make the timer callback
//             function pure virtual?
//
PeerSet::PeerSet (uint256 const& hash, int interval, bool txnData,
    clock_type& clock, beast::Journal journal)
    : m_journal (journal)
    , m_clock (clock)
    , mLock (this, "PeerSet", __FILE__, __LINE__)
    , mHash (hash)
    , mTimerInterval (interval)
    , mTimeouts (0)
    , mComplete (false)
    , mFailed (false)
    , mAggressive (false)
    , mTxnData (txnData)
    , mProgress (false)
    , mTimer (getApp().getIOService ())
{
    mLastAction = m_clock.now();
    assert ((mTimerInterval > 10) && (mTimerInterval < 30000));
}

PeerSet::~PeerSet ()
{
}

bool PeerSet::peerHas (Peer::ref ptr)
{
    ScopedLockType sl (mLock, __FILE__, __LINE__);

    if (!mPeers.insert (std::make_pair (ptr->getShortId (), 0)).second)
        return false;

    newPeer (ptr);
    return true;
}

void PeerSet::badPeer (Peer::ref ptr)
{
    ScopedLockType sl (mLock, __FILE__, __LINE__);
    mPeers.erase (ptr->getShortId ());
}

void PeerSet::setTimer ()
{
    mTimer.expires_from_now (boost::posix_time::milliseconds (mTimerInterval));
    mTimer.async_wait (boost::bind (&PeerSet::TimerEntry, pmDowncast (), boost::asio::placeholders::error));
}

void PeerSet::invokeOnTimer ()
{
    ScopedLockType sl (mLock, __FILE__, __LINE__);

    if (isDone ())
        return;

    if (!isProgress())
    {
        ++mTimeouts;
        WriteLog (lsWARNING, InboundLedger) << "Timeout(" << mTimeouts << ") pc=" << mPeers.size () << " acquiring " << mHash;
        onTimer (false, sl);
    }
    else
    {
        clearProgress ();
        onTimer (true, sl);
    }

    if (!isDone ())
        setTimer ();
}

void PeerSet::TimerEntry (boost::weak_ptr<PeerSet> wptr, const boost::system::error_code& result)
{
    if (result == boost::asio::error::operation_aborted)
        return;

    boost::shared_ptr<PeerSet> ptr = wptr.lock ();

    if (ptr)
    {
        // VFALCO NOTE So this function is really two different functions depending on
        //             the value of mTxnData, which is directly tied to whether we are
        //             a base class of IncomingLedger or TransactionAcquire
        //
        if (ptr->mTxnData)
        {
            getApp().getJobQueue ().addJob (jtTXN_DATA, "timerEntryTxn",
                BIND_TYPE (&PeerSet::TimerJobEntry, P_1, ptr));
        }
        else
        {
            int jc = getApp().getJobQueue ().getJobCountTotal (jtLEDGER_DATA);

            if (jc > 4)
            {
                WriteLog (lsDEBUG, InboundLedger) << "Deferring PeerSet timer due to load";
                ptr->setTimer ();
            }
            else
                getApp().getJobQueue ().addJob (jtLEDGER_DATA, "timerEntryLgr",
                    BIND_TYPE (&PeerSet::TimerJobEntry, P_1, ptr));
	}
    }
}

void PeerSet::TimerJobEntry (Job&, boost::shared_ptr<PeerSet> ptr)
{
    ptr->invokeOnTimer ();
}

bool PeerSet::isActive ()
{
    ScopedLockType sl (mLock, __FILE__, __LINE__);
    return !isDone ();
}

void PeerSet::sendRequest (const protocol::TMGetLedger& tmGL, Peer::ref peer)
{
    if (!peer)
        sendRequest (tmGL);
    else
        peer->sendPacket (boost::make_shared<PackedMessage> (tmGL, protocol::mtGET_LEDGER), false);
}

void PeerSet::sendRequest (const protocol::TMGetLedger& tmGL)
{
    ScopedLockType sl (mLock, __FILE__, __LINE__);

    if (mPeers.empty ())
        return;

    PackedMessage::pointer packet (
        boost::make_shared<PackedMessage> (tmGL, protocol::mtGET_LEDGER));

    for (auto const& p : mPeers)
    {
        Peer::pointer peer (getApp().getPeers ().findPeerByShortID (p.first));

        if (peer)
            peer->sendPacket (packet, false);
    }
}

std::size_t PeerSet::takePeerSetFrom (const PeerSet& s)
{
    std::size_t ret = 0;
    mPeers.clear ();

    for (auto const& p : s.mPeers)
    {
        mPeers.insert (std::make_pair (p.first, 0));
        ++ret;
    }
    
    return ret;
}

std::size_t PeerSet::getPeerCount () const
{
    std::size_t ret (0);

    for (auto const& p : mPeers)
    {
        if (getApp ().getPeers ().findPeerByShortID (p.first))
            ++ret;
    }

    return ret;
}

} // ripple
