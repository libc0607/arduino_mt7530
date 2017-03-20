#define EINVAL 1
#define PIN_MDIO 2
#define PIN_MDC 3

#define MT7530_CPU_PORT         6
#define MT7530_NUM_PORTS        8
#define MT7530_NUM_VLANS        16
#define MT7530_MAX_VID          4095
#define MT7530_MIN_VID          0

#define PHY30_REG13_PORT1_REMOVE_TAG 0x10
#define PHY30_REG13_VLAN_EN 0x8
#define REG_ESW_VLAN_VTCR               0x90
#define REG_ESW_VLAN_VAWD1              0x94
#define REG_ESW_VLAN_VAWD2              0x98
#define REG_ESW_VLAN_VTIM(x)    (0x100 + 4 * ((x) / 2))
#define REG_HWTRAP              0x7804
#define REG_ESW_PORT_PCR(x)     (0x2004 | ((x) << 8))
#define REG_ESW_PORT_PVC(x)     (0x2010 | ((x) << 8))
#define REG_ESW_PORT_PPBV1(x)   (0x2014 | ((x) << 8))
#define BIT(x)                 (1<<x)
#define REG_ESW_VLAN_VAWD1_IVL_MAC      BIT(30)
#define REG_ESW_VLAN_VAWD1_VTAG_EN      BIT(28)
#define REG_ESW_VLAN_VAWD1_VALID        BIT(0)

//==================================================================
//配置：
int global_vlan_enable = 1;
struct mt7530_mapping {
  char    *name;
  u16     pvids[MT7530_NUM_PORTS];
  u8      members[MT7530_NUM_VLANS];
  u8      etags[MT7530_NUM_VLANS];
  u16     vids[MT7530_NUM_VLANS];
} mt7530_defaults = {
  .name = "aaaaa",
  .pvids =    { 1, 1, 1, 1, 1, 1, 1},
  .members =  { 0, 0x7f},   
  .etags =    { 0, 0x40},   
  .vids =     { 0, 1},
};
//==================================================================



uint16_t mt7530_port_entries_pvid[MT7530_NUM_PORTS];

uint8_t switch_port_link;

uint8_t switch_port_duplex;

int32_t switch_port_speed[MT7530_NUM_PORTS];


uint16_t vlan_entries_vid[MT7530_NUM_VLANS];
uint8_t vlan_entries_member[MT7530_NUM_VLANS];
uint8_t vlan_entries_etags[MT7530_NUM_VLANS];


enum {//
  ETAG_CTRL_UNTAG = 0,
  ETAG_CTRL_TAG   = 2,
  ETAG_CTRL_SWAP  = 1,
  ETAG_CTRL_STACK = 3,
};

enum switch_port_speed {
  SWITCH_PORT_SPEED_UNKNOWN = 0,
  SWITCH_PORT_SPEED_10 = 10,
  SWITCH_PORT_SPEED_100 = 100,
  SWITCH_PORT_SPEED_1000 = 1000,
};

void mdio_out_bit(int32_t b)//
{
  digitalWrite(PIN_MDC, LOW);
  if ( b == 0 )
    digitalWrite(PIN_MDIO, LOW);
  else
    digitalWrite(PIN_MDIO, HIGH);
  delayMicroseconds(1);
  digitalWrite(PIN_MDC, HIGH);
  delayMicroseconds(1);
}

int32_t mdio_in_bit()//
{
  digitalWrite(PIN_MDC, LOW);
  delayMicroseconds(1);
  unsigned int res = digitalRead(PIN_MDIO);
  digitalWrite(PIN_MDC, HIGH);
  delayMicroseconds(1);
  return res == HIGH ? 1 : 0;
}

uint16_t mdio_read_reg(uint32_t phyaddr, uint32_t regaddr)//
{
  int32_t k = 0;
  uint16_t res = 0;
  pinMode(PIN_MDC, OUTPUT);
  mdio_in_bit();
  mdio_in_bit();//IDLE
  pinMode(PIN_MDIO, OUTPUT);
  pinMode(PIN_MDC, HIGH);
  mdio_out_bit(0);//START
  mdio_out_bit(1);
  mdio_out_bit(1);//READ
  mdio_out_bit(0);

  for ( k = 4; k >= 0; k-- )
    mdio_out_bit((phyaddr >> k) & 0x1);
  for ( k = 4; k >= 0; k-- )
    mdio_out_bit((regaddr >> k) & 0x1);

  pinMode(PIN_MDIO, INPUT);
  digitalWrite(PIN_MDIO, HIGH); //Pullup
  mdio_in_bit(); //Z
  mdio_in_bit();
  for ( k = 15; k >= 0; k-- )
    res |= ( mdio_in_bit() << k );
  return res;
}

void mdio_write_reg(uint32_t phyaddr, uint32_t regaddr, uint32_t value)//
{
  int32_t k = 0;
  uint32_t res = 0;
  pinMode(PIN_MDC, OUTPUT);
  mdio_in_bit();
  mdio_in_bit();//IDLE
  pinMode(PIN_MDIO, OUTPUT);
  pinMode(PIN_MDC, HIGH);
  mdio_out_bit(0);//START
  mdio_out_bit(1);
  mdio_out_bit(0);//WRITE
  mdio_out_bit(1);

  for ( k = 4; k >= 0; k-- )
    mdio_out_bit((phyaddr >> k) & 0x1);
  for ( k = 4; k >= 0; k-- )
    mdio_out_bit((regaddr >> k) & 0x1);
  mdio_out_bit(1); //TA
  mdio_out_bit(0);

  for ( k = 15; k >= 0; k-- )
    mdio_out_bit((value >> k) & 0x1);

  pinMode(PIN_MDIO, INPUT);
  digitalWrite(PIN_MDIO, HIGH);
  mdio_in_bit();
  mdio_in_bit();//IDLE
}

void mt7530_w32(uint32_t reg, uint32_t val)//
{
  mdio_write_reg(0x1f, 0x1f, (reg >> 6) & 0x3ff);
  mdio_write_reg(0x1f, (reg >> 2) & 0xf,  val & 0xffff);
  mdio_write_reg(0x1f, 0x10, val >> 16);
}

uint32_t mt7530_r32(uint32_t reg)//
{
  mdio_write_reg(0x1f, 0x1f, (reg >> 6) & 0x3ff);
  uint32_t readvalue = (mdio_read_reg(0x1f, 0x10) << 16) | (mdio_read_reg(0x1f, (reg >> 2) & 0xf) & 0xffff);
  return readvalue;
}

void mt7530_vtcr(uint32_t cmd, uint32_t val)//
{
  int32_t i;
  mt7530_w32(REG_ESW_VLAN_VTCR, BIT(31) | (cmd << 12) | val);
  for (i = 0; i < 10; i++) {
    uint32_t val = mt7530_r32(REG_ESW_VLAN_VTCR);
    if ((val & BIT(31)) == 0)
      break;
    delayMicroseconds(1);
  }
  if (i == 10)
    Serial.print("mt7530: vtcr timeout\n");
}

int32_t mt7530_get_port_pvid(int32_t port)//
{
  if (port >= MT7530_NUM_PORTS)
    return -1;

  return mt7530_r32(REG_ESW_PORT_PPBV1(port)) & 0xfff;
}

int32_t mt7530_set_port_pvid(int32_t port, int32_t pvid)//
{
  if (port >= MT7530_NUM_PORTS)
    return -1;

  if (pvid < MT7530_MIN_VID || pvid > MT7530_MAX_VID)
    return -1;

  mt7530_port_entries_pvid[port] = pvid;

  return 0;
}

int32_t mt7530_get_vlan_ports(int32_t port_vlan)
{

  uint32_t member;
  uint32_t etags;
  int i;


  if (port_vlan < 0 || port_vlan >= MT7530_NUM_VLANS)
    return -EINVAL;

  mt7530_vtcr(0, port_vlan); 

  member = mt7530_r32(REG_ESW_VLAN_VAWD1);
  member >>= 16;
  member &= 0xff;

  etags = mt7530_r32(REG_ESW_VLAN_VAWD2);

  for (i = 0; i < MT7530_NUM_PORTS; i++) {
    int etag;

    if (!(member & BIT(i)))
      continue;

    vlan_entries_member[port_vlan] |= BIT(i);

    // 由etags得到每个端口的etag
    etag = (etags >> (i * 2)) & 0x3;

    if (etag == ETAG_CTRL_TAG)
      vlan_entries_etags[port_vlan] |= BIT(i);
    else if (etag != ETAG_CTRL_UNTAG)
      Serial.println("vlan  port: neither untag nor tag.\n");
  }

  return 0;
}

static int mt7530_set_vlan_ports(int32_t port_vlan, uint8_t member, uint8_t etags)
{

  if (port_vlan < 0 || port_vlan >= MT7530_NUM_VLANS )
    return -EINVAL;

  vlan_entries_member[port_vlan] = member;
  vlan_entries_etags[port_vlan] = etags;

  return 0;
}

// 设置vlan的vid
static int mt7530_set_vid(int32_t vlan, int16_t vid)
{

  if (vlan < 0 || vlan >= MT7530_NUM_VLANS)
    return -EINVAL;

  if (vid < MT7530_MIN_VID || vid > MT7530_MAX_VID)
    return -EINVAL;

  vlan_entries_vid[vlan] = vid;
  return 0;
}

// 更新此vlan的vid
static int mt7530_get_vid(int32_t vlan)
{

  uint32_t vid;

  if (vlan < 0 || vlan >= MT7530_NUM_VLANS)
    return -EINVAL;

  vid = mt7530_r32(REG_ESW_VLAN_VTIM(vlan));
  if (vlan & 1)
    vid = vid >> 12;
  vid &= 0xfff;

  vlan_entries_vid[vlan] = vid;
  return 0;
}

//全局设置写入
static int mt7530_apply_config()
{
  uint32_t i, j, val; 

  if (!global_vlan_enable) {
    for (i = 0; i < MT7530_NUM_PORTS; i++)
      mt7530_w32(REG_ESW_PORT_PCR(i), 0x00ff0000);

    for (i = 0; i < MT7530_NUM_PORTS; i++)
      mt7530_w32(REG_ESW_PORT_PVC(i), 0x810000c0);

    return 0;
  }

  /* set all ports as security mode */
  for (i = 0; i < MT7530_NUM_PORTS; i++)
    mt7530_w32(REG_ESW_PORT_PCR(i), 0x00ff0003);

  /* set all ports as user port */
  for (i = 0; i < MT7530_NUM_PORTS; i++)
    mt7530_w32(REG_ESW_PORT_PVC(i), 0x81000000);

  for (i = 0; i < MT7530_NUM_VLANS; i++) {

    /* vid of vlan */
    val = mt7530_r32(REG_ESW_VLAN_VTIM(i));
    if (i % 2 == 0) {
      val &= 0xfff000;
      val |= vlan_entries_vid[i];
    } else {
      val &= 0xfff;
      val |= (vlan_entries_vid[i] << 12);
    }
    mt7530_w32(REG_ESW_VLAN_VTIM(i), val);

    /* vlan port membership */
    if (vlan_entries_member[i])
      mt7530_w32(REG_ESW_VLAN_VAWD1, REG_ESW_VLAN_VAWD1_IVL_MAC |
                 REG_ESW_VLAN_VAWD1_VTAG_EN | (vlan_entries_member[i] << 16) |
                 REG_ESW_VLAN_VAWD1_VALID);
    else
      mt7530_w32(REG_ESW_VLAN_VAWD1, 0);

    /* egress mode */
    val = 0;
    for (j = 0; j < MT7530_NUM_PORTS; j++) {
      if (vlan_entries_etags[i] & BIT(j))
        val |= ETAG_CTRL_TAG << (j * 2);
      else
        val |= ETAG_CTRL_UNTAG << (j * 2);
    }
    mt7530_w32(REG_ESW_VLAN_VAWD2, val);

    /* write to vlan table */
    mt7530_vtcr(1, i);
  }

  /* Port Default PVID */
  for (i = 0; i < MT7530_NUM_PORTS; i++) {
    val = mt7530_r32(REG_ESW_PORT_PPBV1(i));
    val &= ~0xfff;
    val |= mt7530_port_entries_pvid[i];
    mt7530_w32(REG_ESW_PORT_PPBV1(i), val);
  }

  return 0;
}

// 获取状态
static int mt7530_get_port_link(int32_t port)
{
  uint32_t port_speed, pmsr;

  if (port < 0 || port >= MT7530_NUM_PORTS)
    return -1;

  pmsr = mt7530_r32(0x3008 + (0x100 * port));

  if (pmsr & 1)
    switch_port_link |= BIT(port);
  else
    switch_port_link &= ~BIT(port);

  if ((pmsr >> 1) & 1)
    switch_port_duplex |= BIT(port);
  else
    switch_port_duplex &= ~BIT(port);

  port_speed = (pmsr >> 2) & 3;

  switch (port_speed) {
    case 0:
      switch_port_speed[port] = SWITCH_PORT_SPEED_10;
      break;
    case 1:
      switch_port_speed[port] = SWITCH_PORT_SPEED_100;
      break;
    case 2:
    case 3: /* forced gige speed can be 2 or 3 */
      switch_port_speed[port] = SWITCH_PORT_SPEED_1000;
      break;
    default:
      switch_port_speed[port] = SWITCH_PORT_SPEED_UNKNOWN;
      break;
  }

  return 0;
}
void mt7530_apply_mapping()
{
  int i = 0;
  for (i = 0; i < MT7530_NUM_PORTS; i++)
    mt7530_port_entries_pvid[i] = mt7530_defaults.pvids[i];
  for (i = 0; i < MT7530_NUM_VLANS; i++) {
    vlan_entries_member[i] = mt7530_defaults.members[i];
    vlan_entries_etags[i] = mt7530_defaults.etags[i];
    vlan_entries_vid[i] = mt7530_defaults.vids[i];
  }
}
void mt7530_init()
{
  Serial.println("mt7530 init");
  /* magic vodoo */
  int32_t readvalue = mt7530_r32(REG_HWTRAP);
  Serial.print(readvalue);
  if (readvalue !=  0x1117edf) {
    Serial.println("fixing up MHWTRAP register\n");
    mt7530_w32(REG_HWTRAP, 0x1117edf);
  }
  int i;
  for (i = 0; i < MT7530_NUM_VLANS; i++) {
    vlan_entries_vid[i] = i;
  }
  
}

void setup()
{
 
  Serial.begin(115200);
  delay(50);
  Serial.println("serial");
  pinMode(PIN_MDIO, INPUT);
  pinMode(PIN_MDC, INPUT);
  Serial.println("Waiting for the switch chip to start-up ");
  delay(1000);
  mt7530_init();
  Serial.println("apply");
  mt7530_apply_mapping();
  mt7530_apply_config();
}

void loop()
{

  int j = 0;
  for (j=0; j<5; j++) {
    mt7530_get_port_link(j); 
    if(switch_port_link & BIT(j))
      Serial.println(String("")+"Port "+j+": "+switch_port_speed[j]+" Mbps");
    else
    Serial.println(String("")+"Port "+j+": "+"Disconnected");
  }
  
  mt7530_get_vlan_ports(1);
  Serial.println(String("")+"vlan1:id="+vlan_entries_vid[1]+" member="+vlan_entries_member[1]+" etags="+vlan_entries_etags[1]);
  delay(200);
}
