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

#include "../../../beast/beast/unit_test/suite.h"
#include "../../../beast/beast/chrono/manual_clock.h"

namespace ripple {
namespace PeerFinder {

class Livecache_test : public beast::unit_test::suite
{
public:
    beast::manual_clock <clock_type::duration> m_clock;

    // Add the address as an endpoint
    template <class C>
    void add (beast::uint32 index, beast::uint16 port, C& c)
    {
        Endpoint ep;
        ep.hops = 0;
        ep.address = beast::IP::Endpoint (
            beast::IP::AddressV4 (index), port);
        c.insert (ep);
    }

    void testFetch ()
    {
        Livecache <> c (m_clock, beast::Journal());

        add (1, 1, c);
        add (2, 1, c);
        add (3, 1, c);
        add (4, 1, c);
        add (4, 2, c);
        add (4, 3, c);
        add (5, 1, c);
        add (6, 1, c);
        add (6, 2, c);
        add (7, 1, c);

        // VFALCO TODO!!!

        pass();
    }

    void run ()
    {
        testFetch ();
    }
};

BEAST_DEFINE_TESTSUITE(Livecache,peerfinder,ripple);

}
}
