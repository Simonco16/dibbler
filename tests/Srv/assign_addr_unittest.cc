/*
 * Dibbler - a portable DHCPv6
 *
 * author: Tomasz Mrugalski <thomson@klub.com.pl>
 *
 * released under GNU GPL v2 only licence
 *
 */

#include "IPv6Addr.h"
#include "SrvIfaceMgr.h"
#include "SrvCfgMgr.h"
#include "SrvTransMgr.h"
#include "OptDUID.h"
#include "OptStatusCode.h"
#include "SrvOptTA.h"
#include "DHCPConst.h"
#include "HostRange.h"
#include "assign_utils.h"
#include <gtest/gtest.h>

using namespace std;

namespace test {

TEST_F(ServerTest, SARR_single_class) {

    // check that an interface was successfully selected
    string cfg = "iface REPLACE_ME {\n"
                 "  class { pool 2001:db8:123::/64 }\n"
                 "}\n";

    ASSERT_TRUE( createMgrs(cfg) );

    // now generate SOLICIT (with client-id and ia-na)
    SPtr<TSrvMsg> sol = createSolicit(true, true);

    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);

    cout << "Sending SOLICIT" << endl;
    SPtr<TSrvMsg> adv = sendAndReceive(sol, 1);
    ASSERT_TRUE(adv); // check that there is an ADVERTISE response
    EXPECT_EQ(ADVERTISE_MSG, adv->getType());

    SPtr<TSrvOptIA_NA> rcvIA = SPtr_cast<TSrvOptIA_NA>(adv->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    SPtr<TIPv6Addr> minRange = new TIPv6Addr("2001:db8:123::", true);
    SPtr<TIPv6Addr> maxRange = new TIPv6Addr("2001:db8:123::ffff:ffff:ffff:ffff", true);

    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 101, 102,
                            SERVER_DEFAULT_MAX_PREF, SERVER_DEFAULT_MAX_VALID) );

    SPtr<TSrvCfgIface> cfgIface = SrvCfgMgr().getIfaceByID(iface_->getID());
    ASSERT_TRUE(cfgIface);

    cfgIface->firstAddrClass();
    SPtr<TSrvCfgAddrClass> cfgAddrClass = cfgIface->getAddrClass();
    ASSERT_TRUE(cfgAddrClass);

    EXPECT_EQ(0u, cfgAddrClass->getAssignedCount());

    // now generate REQUEST (with client-id and ia-na)
    SPtr<TSrvMsg> req = createRequest(true, true);

    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);

    ASSERT_TRUE(adv->getOption(OPTION_SERVERID));
    req->addOption(adv->getOption(OPTION_SERVERID));

    // ... and get REPLY from the server
    cout << "Sending REQUEST" << endl;
    SPtr<TSrvMsg> reply = sendAndReceive(req, 2);
    ASSERT_TRUE(reply);
    EXPECT_EQ(REPLY_MSG, reply->getType());

    rcvIA = SPtr_cast<TSrvOptIA_NA>(reply->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    // should return T1 = 101, because SERVER_DEFAULT_MIN_T1(5) < 101 < SERVER_DEFAULT_MAX_T1 (3600)
    // should return T2 = 101, because SERVER_DEFAULT_MIN_T1(10) < 102 < SERVER_DEFAULT_MAX_T1 (5400)
    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 101, 102,
                            SERVER_DEFAULT_MAX_PREF, SERVER_DEFAULT_MAX_VALID) );

    EXPECT_EQ(1u, cfgAddrClass->getAssignedCount());

    cout << "Sending RELEASE" << endl;

    // Create Release (with client-id, but without ia-na)
    SPtr<TSrvMsg> rel = createRelease(true, false);
    rel->addOption(req->getOption(OPTION_SERVERID));
    rcvIA->delOption(OPTION_STATUS_CODE);
    rel->addOption(SPtr_cast<TOpt>(rcvIA));

    SPtr<TSrvMsg> releaseReply = sendAndReceive(rel, 3);
    ASSERT_TRUE(releaseReply);
    EXPECT_EQ(REPLY_MSG, releaseReply->getType());

    EXPECT_EQ(0u, cfgAddrClass->getAssignedCount());
}

TEST_F(ServerTest, SARR_single_class_params) {

    // check that an interface was successfully selected
    string cfg = "iface REPLACE_ME {\n"
                 "  t1 1000\n"
                 "  t2 2000\n"
                 "  preferred-lifetime 3000\n"
                 "  valid-lifetime 4000\n"
                 "  class { pool 2001:db8:123::/64 }\n"
                 "}\n";

    ASSERT_TRUE( createMgrs(cfg) );

    // now generate SOLICIT (with client-id, with ia-na)
    SPtr<TSrvMsg> sol = createSolicit(true, true);
    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);

    SPtr<TSrvMsg> adv = sendAndReceive(sol, 1);
    ASSERT_TRUE(adv); // check that there is an ADVERTISE response
    EXPECT_EQ(ADVERTISE_MSG, adv->getType());

    SPtr<TSrvOptIA_NA> rcvIA = SPtr_cast<TSrvOptIA_NA>(adv->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    SPtr<TIPv6Addr> minRange = new TIPv6Addr("2001:db8:123::", true);
    SPtr<TIPv6Addr> maxRange = new TIPv6Addr("2001:db8:123::ffff:ffff:ffff:ffff", true);

    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 1000, 2000, 3000, 4000));

    cout << "REQUEST" << endl;

    // now generate REQUEST (with client-id, with ia-na)
    SPtr<TSrvMsg> req = createRequest(true, true);
    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);

    ASSERT_TRUE(adv->getOption(OPTION_SERVERID));
    req->addOption(adv->getOption(OPTION_SERVERID));

    // ... and get REPLY from the server
    SPtr<TSrvMsg> reply = sendAndReceive(req, 2);
    ASSERT_TRUE(reply);
    EXPECT_EQ(REPLY_MSG, reply->getType());

    rcvIA = SPtr_cast<TSrvOptIA_NA>(reply->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 1000, 2000, 3000, 4000));
}

TEST_F(ServerTest, SARR_inpool_reservation) {

    // check that an interface was successfully selected
    string cfg = "iface REPLACE_ME {\n"
        "  t1 1000\n"
        "  t2 2000\n"
        "  preferred-lifetime 3000\n"
        "  valid-lifetime 4000\n"
        "  class { pool 2001:db8:123::/64 }\n"
        "  client duid 00:01:00:0a:0b:0c:0d:0e:0f {\n"
        "    address 2001:db8:123::babe\n"
        "  }\n"
        "}\n";

    ASSERT_TRUE( createMgrs(cfg) );

    // now generate SOLICIT (with client-id, with ia-na)
    SPtr<TSrvMsg> sol = createSolicit(true, true);
    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);

    SPtr<TSrvMsg> adv = sendAndReceive(sol, 1);
    ASSERT_TRUE(adv); // check that there is an ADVERTISE response
    EXPECT_EQ(ADVERTISE_MSG, adv->getType());

    SPtr<TSrvOptIA_NA> rcvIA = SPtr_cast<TSrvOptIA_NA>(adv->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    SPtr<TIPv6Addr> minRange = new TIPv6Addr("2001:db8:123::babe", true);
    SPtr<TIPv6Addr> maxRange = new TIPv6Addr("2001:db8:123::babe", true);

    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 1000, 2000, 3000, 4000));

    cout << "REQUEST" << endl;

    // now generate REQUEST (with client-id, with ia-na)
    SPtr<TSrvMsg> req = createRequest(true, true);
    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);

    ASSERT_TRUE(adv->getOption(OPTION_SERVERID));
    req->addOption(adv->getOption(OPTION_SERVERID));

    // ... and get REPLY from the server
    SPtr<TSrvMsg> reply = sendAndReceive(req, 2);
    ASSERT_TRUE(reply);
    EXPECT_EQ(REPLY_MSG, reply->getType());

    rcvIA = SPtr_cast<TSrvOptIA_NA>(reply->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 1000, 2000, 3000, 4000));
}

TEST_F(ServerTest, SARR_inpool_reservation_negative) {

    // check that an interface was successfully selected
    string cfg = "iface REPLACE_ME {\n"
        "  t1 1000\n"
        "  t2 2000\n"
        "  preferred-lifetime 3000\n"
        "  valid-lifetime 4000\n"
        "  class { pool 2001:db8:123::/64 }\n"
        "  client duid 00:01:00:00:00:00:00:00:00 {\n" // not our DUID
        "    address 2001:db8:123::babe\n"
        "  }\n"
        "}\n";

    ASSERT_TRUE( createMgrs(cfg) );

    // now generate SOLICIT (with client-id, with ia-na)
    SPtr<TSrvMsg> sol = createSolicit(true, true);
    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);
    SPtr<TIPv6Addr> addr = new TIPv6Addr("2001:db8:123::babe", true);
    SPtr<TOpt> optAddr = new TSrvOptIAAddress(addr, 1000, 2000, &(*sol));
    ia_->addOption(optAddr);

    SPtr<TSrvMsg> adv = sendAndReceive(sol, 1);
    ASSERT_TRUE(adv); // check that there is an ADVERTISE response
    EXPECT_EQ(ADVERTISE_MSG, adv->getType());

    SPtr<TSrvOptIA_NA> rcvIA = SPtr_cast<TSrvOptIA_NA>(adv->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    SPtr<TSrvOptIAAddress> rcvOptAddr = SPtr_cast<TSrvOptIAAddress>(rcvIA->getOption(OPTION_IAADDR));
    ASSERT_TRUE(rcvOptAddr);
    cout << "Requested " << addr->getPlain() << ", received "
         << rcvOptAddr->getAddr()->getPlain() << endl;

    if (addr->getPlain() == rcvOptAddr->getAddr()->getPlain()) {
        ADD_FAILURE() << "Assigned address that was reserved for someone else.";
    }

    SPtr<TIPv6Addr> minRange = new TIPv6Addr("2001:db8:123::", true);
    SPtr<TIPv6Addr> maxRange = new TIPv6Addr("2001:db8:123::ffff:ffff:ffff:ffff", true);

    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 1000, 2000, 3000, 4000));

    cout << "REQUEST" << endl;

    // now generate REQUEST
    SPtr<TSrvMsg> req = createRequest();
    req->addOption(clntId_);
    req->addOption(SPtr_cast<TOpt>(ia_));
    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);

    ASSERT_TRUE(adv->getOption(OPTION_SERVERID));
    req->addOption(adv->getOption(OPTION_SERVERID));

    // ... and get REPLY from the server
    SPtr<TSrvMsg> reply = sendAndReceive(req, 2);
    ASSERT_TRUE(reply);
    EXPECT_EQ(REPLY_MSG, reply->getType());

    rcvIA = SPtr_cast<TSrvOptIA_NA>(reply->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 1000, 2000, 3000, 4000));
}

TEST_F(ServerTest, SARR_inpool_reservation_negative2) {

    // check that if the pool is small and address is reserved for client A, client B
    // will not get it
    string cfg = "iface REPLACE_ME {\n"
        "  t1 1000\n"
        "  t2 2000\n"
        "  preferred-lifetime 3000\n"
        "  valid-lifetime 4000\n"
        "  class { pool 2001:db8:123::babe }\n"
        "  client duid 00:01:00:00:00:00:00:00:00 {\n" // not our DUID
        "    address 2001:db8:123::babe\n"
        "  }\n"
        "}\n";

    ASSERT_TRUE( createMgrs(cfg) );

    // now generate SOLICIT (with client-id, with ia-na)
    SPtr<TSrvMsg> sol = createSolicit(true, true);
    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);
    SPtr<TIPv6Addr> addr = new TIPv6Addr("2001:db8:123::babe", true);
    SPtr<TOpt> optAddr = new TSrvOptIAAddress(addr, 1000, 2000, &(*sol));
    ia_->addOption(optAddr);

    SPtr<TSrvMsg> adv = sendAndReceive(sol, 1);
    ASSERT_TRUE(adv); // check that there is an ADVERTISE response
    EXPECT_EQ(ADVERTISE_MSG, adv->getType());

    SPtr<TSrvOptIA_NA> rcvIA = SPtr_cast<TSrvOptIA_NA>(adv->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    SPtr<TSrvOptIAAddress> rcvOptAddr = SPtr_cast<TSrvOptIAAddress>(rcvIA->getOption(OPTION_IAADDR));
    if (rcvOptAddr) {
        FAIL() << "Client received " << rcvOptAddr->getAddr()->getPlain()
               << " addr, but expected NoAddrsAvail status." << endl;
    }

    SPtr<TOptStatusCode> rcvStatusCode =
        SPtr_cast<TOptStatusCode>(rcvIA->getOption(OPTION_STATUS_CODE));
    ASSERT_TRUE(rcvStatusCode);

    EXPECT_EQ(STATUSCODE_NOADDRSAVAIL, rcvStatusCode->getCode());

}

TEST_F(ServerTest, SARR_outpool_reservation) {

    // check that an interface was successfully selected
    string cfg = "iface REPLACE_ME {\n"
        "  t1 1000\n"
        "  t2 2000\n"
        "  preferred-lifetime 3000\n"
        "  valid-lifetime 4000\n"
        "  class { pool 2001:db8:123::/64 }\n"
        "  client duid 00:01:00:0a:0b:0c:0d:0e:0f {\n"
        "    address 2002::babe\n"
        "  }\n"
        "}\n";

    ASSERT_TRUE( createMgrs(cfg) );

    // now generate SOLICIT
    SPtr<TSrvMsg> sol = createSolicit(true, true);
    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);

    SPtr<TSrvMsg> adv = sendAndReceive(sol, 1);
    ASSERT_TRUE(adv); // check that there is an ADVERTISE response

    SPtr<TSrvOptIA_NA> rcvIA = SPtr_cast<TSrvOptIA_NA>(adv->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);
    EXPECT_EQ(ADVERTISE_MSG, adv->getType());

    SPtr<TIPv6Addr> minRange = new TIPv6Addr("2002::babe", true);
    SPtr<TIPv6Addr> maxRange = new TIPv6Addr("2002::babe", true);

    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 1000, 2000, 3000, 4000));

    cout << "REQUEST" << endl;

    // now generate REQUEST
    SPtr<TSrvMsg> req = createRequest(true, true);
    ia_->setIAID(100);
    ia_->setT1(101);
    ia_->setT2(102);

    ASSERT_TRUE(adv->getOption(OPTION_SERVERID));
    req->addOption(adv->getOption(OPTION_SERVERID));

    // ... and get REPLY from the server
    SPtr<TSrvMsg> reply = sendAndReceive(req, 2);
    ASSERT_TRUE(reply);
    EXPECT_EQ(REPLY_MSG, reply->getType());

    rcvIA = SPtr_cast<TSrvOptIA_NA>(reply->getOption(OPTION_IA_NA));
    ASSERT_TRUE(rcvIA);

    EXPECT_TRUE( checkIA_NA(rcvIA, minRange, maxRange, 100, 1000, 2000, 3000, 4000));
}


}
