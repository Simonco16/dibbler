/*
 * Dibbler - a portable DHCPv6
 *
 * authors: Tomasz Mrugalski <thomson@klub.com.pl>
 *          Marek Senderski <msend@o2.pl>
 * changes: Krzysztof Wnuk <keczi@poczta.onet.pl>
 * released under GNU GPL v2 or later licence
 *
 * $Id: ClntMsgRenew.cpp,v 1.11 2007-02-02 00:52:03 thomson Exp $
 *
 */

#include "ClntMsgRenew.h"
#include "DHCPConst.h"
#include "ClntOptIA_NA.h"
#include "ClntOptIA_PD.h"
#include "ClntOptClientIdentifier.h"
#include "ClntOptServerIdentifier.h"
#include "ClntOptOptionRequest.h"
#include "ClntOptStatusCode.h"
#include "Logger.h"
#include "SmartPtr.h"

#include <cmath>

TClntMsgRenew::TClntMsgRenew(SmartPtr<TClntIfaceMgr> IfaceMgr,
			     SmartPtr<TClntTransMgr> TransMgr,
			     SmartPtr<TClntCfgMgr> CfgMgr,
			     SmartPtr<TClntAddrMgr> AddrMgr,
			     List(TAddrIA) IALst,
			     List(TAddrIA) PDLst)
    :TClntMsg(IfaceMgr,TransMgr,CfgMgr,AddrMgr,
	      IALst.count()?IALst.getFirst()->getIface():PDLst.getFirst()->getIface(), 
	      IALst.count()?IALst.getFirst()->getSrvAddr():PDLst.getFirst()->getSrvAddr(), RENEW_MSG)
{

   // set transmission parameters
    IRT=REN_TIMEOUT;
    MRT=REN_MAX_RT;
    MRC=0;
    RT=0;

    // retransmit until T2 has been reached or any address has expired
    //it shhould be the same for all IAs
    unsigned int timeout = DHCPV6_INFINITY;
    IALst.first();
    MRD= IALst.getFirst()->getT2Timeout();  
    
    if (RT>MRD) 
        RT=MRD;

    // store our DUID
    Options.append(new TClntOptClientIdentifier(CfgMgr->getDUID(),this));

    // and say who's this message is for
    if (IALst.count())
	Options.append( new TClntOptServerIdentifier(IALst.getFirst()->getDUID(),this));
    else
	Options.append( new TClntOptServerIdentifier(PDLst.getFirst()->getDUID(),this));
    
    //Store all IAs to renew
    SmartPtr<TAddrIA> ia;
    IALst.first();
    while(ia=IALst.get()) {
	if (timeout > ia->getT2Timeout())
	    timeout = ia->getT2Timeout();
	Options.append(new TClntOptIA_NA(ia,this));
    }

    PDLst.first();
    while (ia=PDLst.get()) {
	if (timeout > ia->getT2Timeout())
	    timeout = ia->getT2Timeout();
	Options.append(new TClntOptIA_PD(ia, this));
    }

    pkt = new char[getSize()];
    this->IsDone = false;
    this->send();
}

void TClntMsgRenew::answer(SmartPtr<TClntMsg> Reply)
{
    SmartPtr<TOpt> opt;
    
    // get DUID
    SmartPtr<TClntOptServerIdentifier> ptrDUID;
    ptrDUID = (Ptr*) this->getOption(OPTION_SERVERID);
    
    SmartPtr<TClntOptOptionRequest> ptrOptionReqOpt=(Ptr*)getOption(OPTION_ORO);

    Reply->firstOption();
    // for each option in message... (there should be only one IA option, as we send 
    // separate RENEW for each IA, but we check all options anyway)
    while ( opt = Reply->getOption() ) {
        switch (opt->getOptType()) {
	case OPTION_IA_NA: {
	    SmartPtr<TClntOptIA_NA> ptrOptIA = (Ptr*)opt;
	    if (ptrOptIA->getStatusCode()!=STATUSCODE_SUCCESS) {
		SmartPtr<TClntOptStatusCode> status = (Ptr*) ptrOptIA->getOption(OPTION_STATUS_CODE);
		Log(Warning) << "Received IA (iaid=" << ptrOptIA->getIAID() << ") with status code " << 
		    StatusCodeToString(status->getCode()) << ": " 
			     << status->getText() << LogEnd;
		break;
	    }
	    ptrOptIA->setContext(ClntIfaceMgr, ClntTransMgr, ClntCfgMgr, ClntAddrMgr,
			         ptrDUID->getDUID(), SmartPtr<TIPv6Addr>() /*NULL*/, Reply->getIface());

	    ptrOptIA->doDuties();
	    break;
	}
	case OPTION_IA_PD: {
	    SPtr<TClntOptIA_PD> pd = (Ptr*) opt;
	    if (pd->getStatusCode() != STATUSCODE_SUCCESS) {
		SmartPtr<TClntOptStatusCode> status = (Ptr*) pd->getOption(OPTION_STATUS_CODE);
		Log(Warning) << "Received PD (iaid=" << pd->getIAID() << ") with status code " << 
		    StatusCodeToString(status->getCode()) << ": " 
			     << status->getText() << LogEnd;
		break;
	    }
	    pd->setContext(ClntIfaceMgr, ClntTransMgr, ClntCfgMgr, ClntAddrMgr, ptrDUID->getDUID(), 0, (TMsg*)this);
	    pd->doDuties();
	    break;
	}
	case OPTION_ORO:
	case OPTION_RELAY_MSG:
	case OPTION_INTERFACE_ID:
	case OPTION_IAADDR:
	case OPTION_RECONF_MSG:
	    Log(Warning) << "Illegal option (" << opt->getOptType() 
			 << ") in received REPLY message." << LogEnd;
	    break;
	default:
	    // what to do with unknown/other options? execute them
	    opt->setParent(this);
	    opt->doDuties();
	}
    }

    //Here we received answer from our server, which updated the "whole information"
    //There is no use to send Rebind even if server realesed some addresses/IAs
    //in such a case new Solicit message should be sent
    IsDone = true;
}

void TClntMsgRenew::releaseIA(long IAID)
{
    // setStatus(NOTCONFIGURED);
    //FIXME:
}

void TClntMsgRenew::doDuties()
{
    // FIXME: increase RT from REN_TIMEOUT to REN_MAX_RT

    // should we send RENEW once more or start sending REBIND
    if (!MRD) 
    {
	Log(Notice) << "RENEW remains unanswered and timeout T2 reached. " << LogEnd;
        ClntTransMgr->sendRebind(this->Options,this->getIface());
        IsDone = true;
        return;
    }
    //In other case
    //FIXME:IF
    //      in any of renewed IAs all addresses expired (before reaching T2 timeout)
    //      in such Server didn't want IAs to be rebind in other server
    //ELSE
    send();
}


bool TClntMsgRenew::check()
{
    // this should never happen
    return false;
}

string TClntMsgRenew::getName() {
    return "RENEW";
}

TClntMsgRenew::~TClntMsgRenew()
{
    delete pkt;
    pkt = NULL;
}
