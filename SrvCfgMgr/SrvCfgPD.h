/*                                                                           
 * Dibbler - a portable DHCPv6                                               
 *                                                                           
 * author: Krzysztof Wnuk <keczi@poczta.onet.pl>
 *                              
 * released under GNU GPL v2 or later licence                                
 *                                                                           
 * $Id: SrvCfgPD.h,v 1.2 2006-12-04 23:37:53 thomson Exp $
 *
 */


/*
  Generally prefixes can be divided into 3 parts:
  - constant prefix (a)
  - variable section (b) 
  - zeroed tail (c)

       (a)            (b)          (c)  
  aaaa:aaaa:aaaa:bbbb:bbbb:bbbb:0000:0000

  When there are several prefix pools defined, 
  (a) becomes pool-specific prefix
  (b) becomes common part
  (c) stays zeroed tail
*/

class TSrvCfgPD;
#ifndef SRVCONFPD_H
#define SRVCONFPD_H

#include <string>
#include <iostream>
#include <iomanip>

#include "SrvAddrMgr.h"
#include "SrvParsGlobalOpt.h"
#include "DHCPConst.h"
#include "SmartPtr.h"
#include "IPv6Addr.h"
#include "DUID.h"
#include "SmartPtr.h"
#include "SrvCfgPD.h"

using namespace std;


class TSrvCfgPD
{
    friend ostream& operator<<(ostream& out,TSrvCfgPD& iface);
 public:
    TSrvCfgPD();
    
    //Is client with this DUID and IP address supported?
    bool clntSupported(SmartPtr<TDUID> duid,SmartPtr<TIPv6Addr> clntAddr);
    //Is client with this DUID and IP address prefered? (is in accept-only?)
    bool clntPrefered(SmartPtr<TDUID> duid,SmartPtr<TIPv6Addr> clntAddr);
    
    //checks if the prefix belongs to the pool
    bool prefixInPool(SmartPtr<TIPv6Addr> prefix);
    unsigned long countPrefixesInPool();
    SmartPtr<TIPv6Addr> getRandomPrefix();
    List(TIPv6Addr) getRandomList();
    
    unsigned long getT1(unsigned long hintT1);
    unsigned long getT2(unsigned long hintT2);
    unsigned long getPrefered(unsigned long hintPrefered);
    unsigned long getValid(unsigned long hintValid);

    unsigned long getPD_Length(); // length of prefix 
    unsigned long getPD_MaxLease();
    unsigned long getID();

    bool isLinkLocal();

    unsigned long getAssignedCount();
    long incrAssigned(int count=1);
    long decrAssigned(int count=1);

    bool setOptions(SmartPtr<TSrvParsGlobalOpt> opt, int PDPrefix);
    virtual ~TSrvCfgPD();
 private:
    unsigned long PD_T1Beg;
    unsigned long PD_T2Beg;
    unsigned long PD_T1End;
    unsigned long PD_T2End;
    unsigned long PD_Length;     // (shorter) prefix, assigned to the user, e.g. 64
    //unsigned long PD_PoolLength; // (longer) pool length, e.g. 48
    unsigned long PD_PrefBeg;
    unsigned long PD_PrefEnd;
    unsigned long PD_ValidBeg;
    unsigned long PD_ValidEnd;
    
    unsigned long chooseTime(unsigned long beg, unsigned long end, unsigned long clntTime);
    
    unsigned long ID;
    static unsigned long staticID;

    List(TStationRange) PoolLst;
    SPtr<TStationRange> CommonPool; /* common part of all available prefix pools (section b in the description above) */
    unsigned long PD_MaxLease;
    unsigned long PD_Assigned;
    unsigned long PD_Count;
};

#endif