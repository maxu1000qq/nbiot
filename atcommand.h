#ifndef __atcommand_H
#define __atcommand_H
#ifdef __cplusplus
 extern "C" {
#endif

// BC95-B5 AT指令语法
//--------------------|------------------------|-------------------------------------
//  Test Command        AT+<cmd>=?               Check possible sub-parameter values
//  Read Command        AT+<cmd>?                Check current sub-parameter values
//  Set Command         AT+<cmd>=p1[,p2[,p3]]]   Set command
//  Execution Command   AT+<cmd>                 Execution command
//--------------------|------------------------|-------------------------------------
  
// AT 指令响应
// Description:
// When the AT Command processor has finished processing a line,it will output either "OK" or "ERROR"
// indicating that it is ready to accept a new command.Solicited informational responses are sent before the
// final "OK" or "ERROR".Unsolicited information response will never occur between a solicited
// informational response and the final "OK" or "ERROR"

// Responses will be of the format:
// <CR><LF>+CMD1:<parameters><CR><LF>
// <CR><LF>OK<CR><LF>
// Or
// <CR><LF><parameters><CR><LF>
// <CR><LF>OK<CR><LF>
//  
  
//BC95_B5支持的AT指令说明
/* AT Command Description Implementation Status */
  
/* 3GPP Commands (27.007)*/
//------------|-----------------------------------------------------| 
// AT+CGMI    | Request Manufacturer Identification B350 or later   
//------------|-----------------------------------------------------|
// AT+CGMM    | Request Manufacturer Model B350 or later            
//------------|-----------------------------------------------------|
// AT+CGMR    | Request Manufacturer Revision B350 or later         
//------------|-----------------------------------------------------|
// AT+CGSN    | Request Product Serial Number B350 or later         
//------------|-----------------------------------------------------|
// AT+CEREG   | EPS Network Registration Status B350 or later       
//------------|-----------------------------------------------------|
// AT+CSCON   | Signalling Connection Status B350 or later
//------------|-----------------------------------------------------|
// AT+CLAC    | List Available Commands B350 or later
//------------|-----------------------------------------------------|
// AT+CSQ     | Get Signal Strength Indicator B350 or later
//------------|-----------------------------------------------------|
// AT+CGPADDR | Show PDP Addresses B350 or later
//------------|-----------------------------------------------------|
// AT+COPS    | PLMN Selection B350 or later
//------------|-----------------------------------------------------|
// AT+CGATT   | PS Attach or Detach B350 or later
//------------|-----------------------------------------------------|
// AT+CGACT   | Activate or Deactivate PDP Context B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CIMI    | Request International Mobile Subscriber Identity B350 or later
//------------|-----------------------------------------------------|
// AT+CGDCONT | Define a PDP Context B350 or later
//------------|-----------------------------------------------------|
// AT+CFUN    | Set Phone Functionality B350 or later
//------------|-----------------------------------------------------|
// AT+CMEE    | Report Mobile Termination Error B600 or later
//------------|-----------------------------------------------------|
// AT+CCLK    | Return Current Date & Time B656 or later
//------------|-----------------------------------------------------|
// AT+CPSMS   | Power Saving Mode Setting B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CEDRXS  | eDRX Setting B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CEER    | Extended Error Report B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CEDRXRDP| eDRX Read Dynamic Parameters B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CTZR    | Time Zone Reporting B657SP1 or later
//------------|-----------------------------------------------------|

/*   ETSI Commands* (127.005)  <Under development> */
//------------|-----------------------------------------------------|
// AT+CSMS    | Select Message Service B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CNMA    | New Message Acknowledgement to ME/TA B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CSCA    | Service Centre Address B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CMGS    | Send SMS Message B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CMGC    | Send SMS Command B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CSODCP  | Sending of Originating Data via the Control Plane B657SP1 or later
//------------|-----------------------------------------------------|
// AT+CRTDCP  | Reporting of Terminating Data via the Control Plane
//------------|-----------------------------------------------------|

/*   General Commands  */
//--------------|-----------------------------------------------------|
// AT+NMGS      | Send a Message B350 or later
//--------------|-----------------------------------------------------|
// AT+NMGR      | Get a Message B350 or later
//--------------|-----------------------------------------------------|
// AT+NNMI      | New Message Indications B350 or later
//--------------|-----------------------------------------------------|
// AT+NSMI      | Sent message Indications B350 or later
//--------------|-----------------------------------------------------|
// AT+NQMGR     | Query Messages Received B350 or later
//--------------|-----------------------------------------------------|
// AT+NQMGS     | Query Messages Sent B350 or later
//--------------|-----------------------------------------------------|
// AT+NMSTATUS  | Message Registration Status B657SP1 or later
//--------------|-----------------------------------------------------|
// AT+NRB       | Reboot B350 or later
//--------------|-----------------------------------------------------|
// AT+NCDP      | Configure and Query CDP Server Settings B350 or later
//--------------|-----------------------------------------------------|
// AT+NUESTATS  | Query UE Statistics B350 or later
//--------------|-----------------------------------------------------|
// AT+NEARFCN   | Specify Search Frequencies B350 or later
//--------------|-----------------------------------------------------|
// AT+NSOCR     | Create a Socket B350 or later
//--------------|-----------------------------------------------------|
// AT+NSOST     | SendTo Command (UDP Only) B350 or later
//--------------|-----------------------------------------------------|
// AT+NSOSTF    | SendTo Command with Flags (UDP Only) B656 or later
//--------------|-----------------------------------------------------|
// AT+NSORF     | Receive Command (UDP only) B350 or later
//--------------|-----------------------------------------------------|
// AT+NSOCL     | Close a Socket B350 or later
//--------------|-----------------------------------------------------|
// +NSONMI      | Socket Message Arrived Indicator (Response Only) B350 or later
//--------------|-----------------------------------------------------|
// AT+NPING     | Test IP Network Connectivity to a Remote Host B350 or later
//--------------|-----------------------------------------------------|
// AT+NBAND     | Set Supported Bands B600 or later
//--------------|-----------------------------------------------------|
// AT+NLOGLEVEL | Set Debug Logging Level B600 or later
//--------------|-----------------------------------------------------|
// AT+NCONFIG   | Configure UE Behaviour B650 or later
//--------------|-----------------------------------------------------|
// AT+NATSPEED  | Configure UART Port Baud Rate B656 or later
//--------------|-----------------------------------------------------|
// AT+NCCID     | Card Identification B657SP1 or later
//--------------|-----------------------------------------------------|
// AT+NFWUPD    | Firmware Update via UART B657SP1 or later
//--------------|-----------------------------------------------------|
// AT+NRDCTRL   | Control Radio Configurations B657SP1 or later
//--------------|-----------------------------------------------------|
// AT+NCHIPINFO | Read System Information B657SP1 or later
//--------------|-----------------------------------------------------|
  
/*  Temporary Commands     */
//--------------|-----------------------------------------------------|
// AT+NTSETID   | Set ID B350 or later
//--------------|-----------------------------------------------------|

/*
 * 常量定义区域
 */
const char *AT_SYNC      = "AT";
const char *AT_CGMI      = "AT+CGMI";
const char *AT_CGMM      = "AT+CGMM";
const char *AT_CGMR      = "AT+CGMR";
const char *AT_CGSN      = "AT+CGSN";
const char *AT_CEREG     = "AT+CEREG";
const char *AT_CSCON     = "AT+CSCON";
const char *AT_CLAC      = "AT+CLAC";
const char *AT_CSQ       = "AT+CSQ";
const char *AT_CGPADDR   = "AT+CGPADDR";
const char *AT_COPS      = "AT+COPS";
const char *AT_CGATT     = "AT+CGATT";
const char *AT_CGACT     = "AT+CGACT";
const char *AT_CIMI      = "AT+CIMI";
const char *AT_CGDCONT   = "AT+CGDCONT";
const char *AT_CFUN      = "AT+CFUN";
const char *AT_CMEE      = "AT+CMEE";
const char *AT_CCLK      = "AT+CCLK";
const char *AT_CPSMS     = "AT+CPSMS";
const char *AT_CEDRXS    = "AT+CEDRXS";
const char *AT_CEER      = "AT+CEER";
const char *AT_CEDRXRDP  = "AT+CEDRXRDP";
const char *AT_CTZR      = "AT+CTZR";

/* ETSI Commands* (127.005)  <Under development> */
/*
const char *AT_CSMS     = "AT+CSMS";
const char *AT_CNMA     = "AT+CNMA";
const char *AT_CSCA     = "AT+CSCA";
const char *AT_CMGS     = "AT+CMGS";
const char *AT_CMGC     = "AT+CMGC";
const char *AT_CSODCP   = "AT+CSODCP";
const char *AT_CRTDCP   = "AT+CRTDCP";
*/
const char *AT_NMGS     = "AT+NMGS";
const char *AT_NMGR     = "AT+NMGR";
const char *AT_NNMI     = "AT+NNMI";
const char *AT_NSMI     = "AT+NSMI";
const char *AT_NQMGR    = "AT+NQMGR";
const char *AT_NQMGS    = "AT+NQMGS";
const char *AT_NMSTATUS = "AT+NMSTATUS";
const char *AT_NRB      = "AT+NRB";
const char *AT_NCDP     = "AT+NCDP";

const char *AT_NUESTATS = "AT+NUESTATS";

const char *AT_NEARFCN  = "AT+NEARFCN";
const char *AT_NSOCR    = "AT+NSOCR";
const char *AT_NSOST    = "AT+NSOST";
const char *AT_NSOSTF   = "AT+NSOSTF";
const char *AT_NSORF    = "AT+NSORF";
const char *AT_NSOCL    = "AT+NSOCL";  

// const char*= "+NSONMI";      
const char *AT_NPING        = "AT+NPING";
const char *AT_NBAND        = "AT+NBAND";
const char *AT_NLOGLEVEL    = "AT+NLOGLEVEL";
const char *AT_NCONFIG      = "AT+NCONFIG";
const char *AT_NATSPEED     = "AT+NATSPEED";
const char *AT_NCCID        = "AT+NCCID";
const char *AT_NFWUPD       = "AT+NFWUPD";
const char *AT_NRDCTRL      = "AT+NRDCTRL";
const char *AT_NCHIPINFO    = "AT+NCHIPINFO";
const char *AT_NTSETID      = "AT+NTSETID";

//==============================================================================
//

#define CMD_TRY_TIMES           5
#define CMD_READ_ARGUMENT       "?"
#define CMD_TEST_ARGUMENT       "=?"

#ifdef __cplusplus
}
#endif
#endif

