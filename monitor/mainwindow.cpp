#include "mainwindow.h"
#include "ui_mainwindow.h"

#define    REC_BUFF_MAX_LEN           (1024)
#define    REC_HEADER_LEN             (4)

#define    OFFSET_DATA_AREA           ( (REC_HEADER_LEN) + 11 )
#define    OFFSET_SINGLE_BAT_INFO     (23)

static BatInfoDef bat_info_buff[BAT_NUM];
static quint8 rec_buff[REC_BUFF_MAX_LEN] = {0};

void initBatInfoBuffer(void)
{
    for( quint16 idx = 0; idx < BAT_NUM; idx++ )
    {
        memset( &bat_info_buff[idx].is_bms_conn, 0, sizeof(BatInfoDef) );
    }
}

uint16_t crc16(uint8_t *buffer, uint16_t buffer_length)
{
    uint8_t crc_hi = 0xFF; /* high CRC byte initialized */
    uint8_t crc_lo = 0xFF; /* low CRC byte initialized */
    unsigned int i; /* will index into CRC lookup */

    /* pass through message buffer */
    while (buffer_length--) {
        i = crc_hi ^ *buffer++; /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }

    return (crc_hi << 8 | crc_lo);
}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    loadSettings();
    initBatInfoBuffer();

    MainWindow::QTextDispTab[ 0 ] = ui->textBrowser_Unit1;
    MainWindow::QTextDispTab[ 1 ] = ui->textBrowser_Unit2;
    MainWindow::QTextDispTab[ 2 ] = ui->textBrowser_Unit3;
    MainWindow::QTextDispTab[ 3 ] = ui->textBrowser_Unit4;
    MainWindow::QTextDispTab[ 4 ] = ui->textBrowser_Unit5;
    MainWindow::QTextDispTab[ 5 ] = ui->textBrowser_Unit6;
    MainWindow::QTextDispTab[ 6 ] = ui->textBrowser_Unit7;
    MainWindow::QTextDispTab[ 7 ] = ui->textBrowser_Unit8;
    MainWindow::QTextDispTab[ 8 ] = ui->textBrowser_Unit9;
    MainWindow::QTextDispTab[ 9 ] = ui->textBrowser_Unit10;
    MainWindow::QTextDispTab[ 10 ] = ui->textBrowser_Unit11;
    MainWindow::QTextDispTab[ 11 ] = ui->textBrowser_Unit12;
    MainWindow::QTextDispTab[ 12 ] = ui->textBrowser_Unit13;
    MainWindow::QTextDispTab[ 13 ] = ui->textBrowser_Unit14;
    MainWindow::QTextDispTab[ 14 ] = ui->textBrowser_Unit15;
    MainWindow::QTextDispTab[ 15 ] = ui->textBrowser_Unit16;

    // 清空电池缓存信息
    memset( &bat_info_buff[0], 0, sizeof(bat_info_buff) );

    // create query timer
    QueryTimer = new QTimer(this);

    // create monitor timer
    MonitorTimer = new QTimer(this);

    // current sys time
    CurrentSysTime = new QTimer(this);
    CurrentSysTime->start(CURRENT_SYS_TIME_OUT);
    connect(CurrentSysTime, SIGNAL(timeout()), this, SLOT(onTimeout_DispSysTime()));
    MainWindow::onTimeout_DispSysTime();

    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));
}

MainWindow::~MainWindow()
{
    delete ui;
}




/******************************************************************************
 ******************************************************************************
 **
 ** TCP Client
 **
 ******************************************************************************
 ******************************************************************************/

/***********************************
 *
 * TCP client start button clicked
 *
 ***********************************/
void MainWindow::onTcpClientButtonClicked()
{
    disconnect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));

    if (setupConnection(TCPCLIENT))
    {
        ui->statusBar->showMessage(messageTCP + "Connecting to " + tcpClientTargetAddr.toString() + ": " + QString::number(tcpClientTargetPort), 0);
        ui->lineEdit_TcpClientTargetIP->setDisabled(true);
        ui->lineEdit_TcpClientTargetPort->setDisabled(true);
        ui->button_TcpClient->setText("停止");

        connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientStopButtonClicked()));
        connect(mytcpclient, SIGNAL(myClientConnected(QString, quint16)), this, SLOT(onTcpClientNewConnection(QString, quint16)));
        connect(mytcpclient, SIGNAL(connectionFailed()), this, SLOT(onTcpClientTimeOut()));

        // start query timer
        QueryTimer->start( QUERY_TIME_OUT );
        qDebug("start query timer");
        connect(QueryTimer, SIGNAL(timeout()),this, SLOT(onTcpClientSendMessage()));
        connect(this, SIGNAL(QueryTimeout()), SLOT(onTcpClientSendMessage()));

        // start monitor timer
        MonitorTimer->start( MONITOR_TIME_OUT );
        memset(&m_monitor_time, 0, sizeof(m_monitor_time));
        connect(MonitorTimer, SIGNAL(timeout()),this, SLOT(onTimeout_Monitor()));
        MainWindow::onTimeout_Monitor();

//        QString detail = "0时0分0秒";
//        ui->lineEdit_MonitorTime->setAlignment(Qt::AlignRight);
//        ui->lineEdit_MonitorTime->setText(detail);
    }

//    saveSettings();
}

/***********************************
 *
 * TCP client has a new connection
 *
 ***********************************/
void MainWindow::onTcpClientNewConnection(const QString &from, quint16 port)
{
    disconnect(mytcpclient, SIGNAL(myClientConnected(QString, quint16)), this, SLOT(onTcpClientNewConnection(QString, quint16)));
    disconnect(mytcpclient, SIGNAL(connectionFailed()), this, SLOT(onTcpClientTimeOut()));
    disconnect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientStopButtonClicked()));
    connect(mytcpclient, SIGNAL(myClientDisconnected()), this, SLOT(onTcpClientDisconnected()));

    ui->button_TcpClient->setDisabled(false);
    ui->button_TcpClient->setText("停止");

//    ui->button_TcpClient->setStyleSheet("background-color:rgb(255,0,0);");

//    ui->button_TcpClientSend->setDisabled(false);
//    ui->lineEdit_TcpClientSend->setDisabled(false);
//    ui->textBrowser_TcpClientMessage->setDisabled(false);

    ui->statusBar->showMessage(messageTCP + "Connected to " + from + ": " + QString::number(port), 0);
    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientDisconnectButtonClicked()));

    connect(mytcpclient, SIGNAL(newMessage(QString, QByteArray)), this, SLOT(onTcpClientAppendMessage(QString, QByteArray)));
//    connect(ui->button_TcpClientSend, SIGNAL(clicked()), this, SLOT(onTcpClientSendMessage()));
//    connect(ui->lineEdit_TcpClientSend, SIGNAL(returnPressed()), this, SLOT(onTcpClientSendMessage()));

    emit QueryTimeout();
    qDebug("emit query timeout signal");

    if( !MonitorTimer->isActive() )
    {
        // start monitor timer
        MonitorTimer->start( MONITOR_TIME_OUT );
        memset(&m_monitor_time, 0, sizeof(m_monitor_time));
        connect(MonitorTimer, SIGNAL(timeout()),this, SLOT(onTimeout_Monitor()));
        MainWindow::onTimeout_Monitor();

//        QString detail = "0时0分0秒";
//        ui->lineEdit_MonitorTime->setAlignment(Qt::AlignRight);
//        ui->lineEdit_MonitorTime->setText(detail);
    }
}

/***********************************
 *
 * TCP client stop button clicked
 *
 ***********************************/
void MainWindow::onTcpClientStopButtonClicked()
{
    disconnect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientStopButtonClicked()));

    ui->statusBar->showMessage(messageTCP + "Stopped", 2000);
    disconnect(mytcpclient, SIGNAL(myClientConnected(QString, quint16)), this, SLOT(onTcpClientNewConnection(QString, quint16)));
    disconnect(mytcpclient, SIGNAL(connectionFailed()), this, SLOT(onTcpClientTimeOut()));
    ui->button_TcpClient->setText("启动");
    mytcpclient->abortConnection();
    ui->lineEdit_TcpClientTargetIP->setDisabled(false);
    ui->lineEdit_TcpClientTargetPort->setDisabled(false);

//    ui->button_TcpClientSend->setDisabled(true);
//    ui->lineEdit_TcpClientSend->setDisabled(true);
//    ui->textBrowser_TcpClientMessage->setDisabled(true);

    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));

    // stop query timer
    QueryTimer->stop();
    qDebug("stop query timer");
    disconnect(QueryTimer, SIGNAL(timeout()),this, SLOT(onTcpClientSendMessage()));

    // stop monitor timer
    MonitorTimer->stop();
    memset(&m_monitor_time, 0, sizeof(m_monitor_time));
    disconnect(MonitorTimer, SIGNAL(timeout()),this, SLOT(onTimeout_Monitor()));
    // 清空显示区域
    ui->lineEdit_MonitorTime->clear();
}

/***********************************
 *
 * TCP client connection time out
 *
 ***********************************/
void MainWindow::onTcpClientTimeOut()
{
    ui->statusBar->showMessage(messageTCP + "Connection time out", 2000);
    disconnect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientStopButtonClicked()));
    disconnect(mytcpclient, SIGNAL(myClientConnected(QString, quint16)), this, SLOT(onTcpClientNewConnection(QString, quint16)));
    disconnect(mytcpclient, SIGNAL(connectionFailed()), this, SLOT(onTcpClientTimeOut()));

    ui->button_TcpClient->setText("启动");
    ui->lineEdit_TcpClientTargetIP->setDisabled(false);
    ui->lineEdit_TcpClientTargetPort->setDisabled(false);

    mytcpclient->closeClient();
    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));

    // stop query timer
    QueryTimer->stop();
    qDebug("stop query timer");
    disconnect(QueryTimer, SIGNAL(timeout()),this, SLOT(onTcpClientSendMessage()));

    // stop monitor timer
    MonitorTimer->stop();
    memset(&m_monitor_time, 0, sizeof(m_monitor_time));
    disconnect(MonitorTimer, SIGNAL(timeout()),this, SLOT(onTimeout_Monitor()));
    // 清空显示区域
    ui->lineEdit_MonitorTime->clear();
}

/***********************************
 *
 * TCP client diconnect button clicked
 *
 ***********************************/
void MainWindow::onTcpClientDisconnectButtonClicked()
{
    mytcpclient->disconnectCurrentConnection();
}

/***********************************
 *
 * TCP client disconnected
 *
 ***********************************/
void MainWindow::onTcpClientDisconnected()
{
    ui->statusBar->showMessage(messageTCP + "Disconnected from server", 2000);
    disconnect(mytcpclient, SIGNAL(myClientDisconnected()), this, SLOT(onTcpClientDisconnected()));
    disconnect(mytcpclient, SIGNAL(newMessage(QString, QByteArray)), this, SLOT(onTcpClientAppendMessage(QString, QByteArray)));
//    disconnect(ui->button_TcpClientSend, SIGNAL(clicked()), this, SLOT(onTcpClientSendMessage()));
//    disconnect(ui->lineEdit_TcpClientSend, SIGNAL(returnPressed()), this, SLOT(onTcpClientSendMessage()));
    disconnect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientDisconnectButtonClicked()));
    ui->button_TcpClient->setText("启动");
//    ui->button_TcpClientSend->setDisabled(true);
//    ui->lineEdit_TcpClientSend->setDisabled(true);
//    ui->textBrowser_TcpClientMessage->setDisabled(true);

    ui->button_TcpClient->setDisabled(false);
    ui->lineEdit_TcpClientTargetIP->setDisabled(false);
    ui->lineEdit_TcpClientTargetPort->setDisabled(false);

    mytcpclient->closeClient();
    mytcpclient->close();

    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));

    // 清空显示区域
    for(quint16 idx = 0; idx < BAT_NUM; idx++ )
    {
        MainWindow::QTextDispTab[idx]->clear();
    }
    ui->lineEdit_MonitorTime->clear();

    // stop query timer
    QueryTimer->stop();
    qDebug("stop query timer");
    disconnect(QueryTimer, SIGNAL(timeout()),this, SLOT(onTcpClientSendMessage()));

    // stop monitor timer
    MonitorTimer->stop();
    qDebug("stop monitor timer");
    memset(&m_monitor_time, 0, sizeof(m_monitor_time));
    disconnect(MonitorTimer, SIGNAL(timeout()),this, SLOT(onTimeout_Monitor()));
}

quint16 get_u16_data_low_byte( quint8 *addr )
{
    return (quint16)( ( addr[0] & 0xFF ) | \
            ( (quint16)( ( addr[1] & 0xFF ) << 8 ) & 0xFF00 ) );
}

bool frame_parse(const QByteArray &message)
{
    quint16 frame_len = 0;   // 报文长度，从报文长度高字节之后，CRC校验低字节之前的部分

    // 把所有数据读取出来
    QByteArray::const_iterator it = message.begin();
    for( quint16 idx=0 ; it != message.end(); it++, idx++ )
    {
        rec_buff[idx] = (quint8)(*it);
    }

    frame_len = rec_buff[4] & 0xff;
    frame_len |= ( ( (rec_buff[5] & 0xff) << 8) & 0xFF00 );
    qDebug("whole frame size:%d, frame_len: %d", message.size(), frame_len);

    // 校验帧头
    quint8 buff_header_std[] = {0x55, 0xAA, 0xA5, 0x5A};
    for( quint16 idx = 0; idx < 4; idx ++ )
    {
        qDebug("chk header: %02x", rec_buff[idx] );
        if( buff_header_std[idx] != rec_buff[idx] )
        {
            qDebug() << "header error";
            return false;
        }
    }

    // 校验crc值
    quint16 crc_read = 0;
    quint16 crc_calc = 0;

    crc_calc = crc16( &rec_buff[4], frame_len+2  );
    crc_read = (rec_buff[frame_len+2+4] & 0xFF) | \
               ( ( ( rec_buff[frame_len+2+4+1] & 0xFF ) << 8) & 0xFF00 );
    if( crc_calc != crc_read )
    {
        qDebug("crc check failed, crc_read:%04X, crc_calc:%04X", crc_read, crc_calc);
        return false;
    }
    else
    {
        qDebug("crc check success, crc_read:%04X, crc_calc:%04X", crc_read, crc_calc);
    }

    // 获取电池详细信息
    quint16 chgr_station_addr = 0;
    quint16 bat_chl = 0;
    quint16 loop_cnt = 0;
    quint16 rec_data_base_idx = 0;
    quint16 bat_info_idx = 0;
    quint16 u16_data;

    chgr_station_addr = ( rec_buff[REC_HEADER_LEN + 5] & 0xFF ) | \
                        ( ( ( rec_buff[REC_HEADER_LEN + 6] & 0xFF ) << 8 ) & 0xFF00 );

    bat_chl = ( rec_buff[REC_HEADER_LEN + 7] & 0xFF ) | \
                        ( ( ( rec_buff[REC_HEADER_LEN + 8] & 0xFF ) << 8 ) & 0xFF00 );

    qDebug("充电站地址:%04X, 通道地址：%04X", chgr_station_addr, bat_chl );

    // 表示获取所有通道电池信息
    if( bat_chl == 0xFFFF)
        loop_cnt = BAT_NUM;
    else  // 表示获取单个通道的电池信息
        loop_cnt = 1;

    // 解析所需数据
    for(quint16 idx = 0; idx < loop_cnt; idx ++)
    {
        // 更新接收缓冲区中，对应的电池信息存储在哪个位置
        rec_data_base_idx = OFFSET_DATA_AREA + ( OFFSET_SINGLE_BAT_INFO * idx );

        // 更新本地存储的电池信息索引
        if( bat_chl == 0xFFFF )
            bat_info_idx = idx;
        else
            bat_info_idx = bat_chl - 1;

        // 判断电池是否连接
        if( rec_buff[rec_data_base_idx] & (quint8)bit(IDX_BIT_IS_BAT_CONN) )
            bat_info_buff[ bat_info_idx ].is_bat_conn = true;
        else
            bat_info_buff[ bat_info_idx ].is_bat_conn = false;

        // 判断电池是否反接
        if( rec_buff[rec_data_base_idx] & (quint8)bit(IDX_BIT_IS_BAT_REVERSE) )
            bat_info_buff[ bat_info_idx ].is_bat_reverse = true;
        else
            bat_info_buff[ bat_info_idx ].is_bat_reverse = false;

        // 判断电池是否在充电
        if( rec_buff[rec_data_base_idx] & (quint8)bit(IDX_BIT_IS_BAT_CHGRING) )
            bat_info_buff[ bat_info_idx ].is_bat_chgring = true;
        else
            bat_info_buff[ bat_info_idx ].is_bat_chgring = false;

        // 获取bms连接状态
        if( rec_buff[rec_data_base_idx] & (quint8)bit(IDX_BIT_IS_BMS_CONN) )
            bat_info_buff[ bat_info_idx ].is_bms_conn = true;
        else
        {
            // 如果没有连接，下面的数据不用解析了
            bat_info_buff[ bat_info_idx ].is_bms_conn = false;
            qDebug("通道%d电池没有连接", bat_info_idx + 1);
            continue;
        }
        // 判断电池是否充满
        if( rec_buff[rec_data_base_idx] & (quint8)bit(IDX_BIT_IS_BAT_FULL) )
            bat_info_buff[ bat_info_idx ].is_bat_full = true;
        else
            bat_info_buff[ bat_info_idx ].is_bat_full = false;

        // 获取温度1
        u16_data = get_u16_data_low_byte( &rec_buff[rec_data_base_idx + 1] );
        bat_info_buff[ bat_info_idx ].tmp_1 = (float)u16_data / 10;
        qDebug("通道%d电池温度1：%f, u16_data:%04X", bat_info_idx + 1, bat_info_buff[ bat_info_idx ].tmp_1, u16_data );
        // 获取温度2
        u16_data = get_u16_data_low_byte( &rec_buff[rec_data_base_idx + 3] );
        bat_info_buff[ bat_info_idx ].tmp_2 = (float)u16_data / 10;
        qDebug("通道%d电池温度2：%f, u16_data:%04X", bat_info_idx + 1, bat_info_buff[ bat_info_idx ].tmp_2, u16_data );
        // 获取单体最高电压
        u16_data = get_u16_data_low_byte( &rec_buff[rec_data_base_idx + 5] );
        bat_info_buff[ bat_info_idx ].single_volt_max = (float)u16_data / 1000;
        qDebug("通道%d单体最高电压：%f, u16_data:%04X", bat_info_idx + 1, bat_info_buff[ bat_info_idx ].single_volt_max, u16_data );
        // 获取单体最低电压
        u16_data = get_u16_data_low_byte( &rec_buff[rec_data_base_idx + 7] );
        bat_info_buff[ bat_info_idx ].single_volt_min =(float) u16_data / 1000;
        qDebug("通道%d单体最低电压：%f, u16_data:%04X", bat_info_idx + 1, bat_info_buff[ bat_info_idx ].single_volt_min, u16_data );
        // 获取电池组电压
        u16_data = get_u16_data_low_byte( &rec_buff[rec_data_base_idx + 9] );
        bat_info_buff[ bat_info_idx ].bat_volt = (float)u16_data / 100;
        qDebug("通道%d电池组电压：%f, u16_data:%04X", bat_info_idx + 1, bat_info_buff[ bat_info_idx ].bat_volt, u16_data );
        // 获取电池组放电电流
        u16_data = get_u16_data_low_byte( &rec_buff[rec_data_base_idx + 11] );
        bat_info_buff[ bat_info_idx ].bat_discharge_current = (float)u16_data / 10;
        qDebug("通道%d电池组放电电流：%f, u16_data:%04X", bat_info_idx + 1, bat_info_buff[ bat_info_idx ].bat_discharge_current, u16_data );
        // 获取电池组充电电流
        u16_data = get_u16_data_low_byte( &rec_buff[rec_data_base_idx + 13] );
        bat_info_buff[ bat_info_idx ].bat_chgr_current = (float)u16_data / 10;
        qDebug("通道%d电池组充电电流：%f, u16_data:%04X", bat_info_idx + 1, bat_info_buff[ bat_info_idx ].bat_chgr_current, u16_data );
        // 获取剩余容量
        u16_data = get_u16_data_low_byte( &rec_buff[rec_data_base_idx + 15] );
        bat_info_buff[ bat_info_idx ].cap_remain = (float)u16_data / 10;
        qDebug("通道%d剩余容量：%f, u16_data:%04X", bat_info_idx + 1, bat_info_buff[ bat_info_idx ].cap_remain, u16_data );
        // 获取限制电流
        u16_data = get_u16_data_low_byte( &rec_buff[rec_data_base_idx + 17] );
        bat_info_buff[ bat_info_idx ].iout_limit = ( (float)u16_data / 1000 ) * 30;
        qDebug("通道%d限制电流：%f, u16_data:%04X", bat_info_idx + 1, bat_info_buff[ bat_info_idx ].iout_limit, u16_data );
        // 获取bms告警字1 - 4
        bat_info_buff[ bat_info_idx ].warn_1 = rec_buff[rec_data_base_idx + 19];
        bat_info_buff[ bat_info_idx ].warn_2 = rec_buff[rec_data_base_idx + 20];
        bat_info_buff[ bat_info_idx ].warn_3 = rec_buff[rec_data_base_idx + 21];
        bat_info_buff[ bat_info_idx ].warn_4 = rec_buff[rec_data_base_idx + 22];
        qDebug("通道%dBMS告警字:%02X, %02X, %02X, %02X, ", bat_info_idx + 1, \
               bat_info_buff[ bat_info_idx ].warn_1,\
               bat_info_buff[ bat_info_idx ].warn_2,\
               bat_info_buff[ bat_info_idx ].warn_3,\
               bat_info_buff[ bat_info_idx ].warn_4 );
    }

    return true;
}

void MainWindow::disp_bat_info(void)
{
    QString detail = "";

    for(quint16 idx = 0; idx < BAT_NUM; idx++ )
    {
        detail = "";
        QTextCursor cursor(MainWindow::QTextDispTab[idx]->textCursor());
        cursor.movePosition(QTextCursor::End);

        //  显示电池连接状态
        if( bat_info_buff[idx].is_bat_conn != true )
        {
            if( bat_info_buff[idx].is_bat_reverse != true )
            {
                detail = detail + MainWindow::tr("电池连接状态： ") + "未连接" + "\n";
            }
            else
            {
                detail = detail + MainWindow::tr("电池连接状态： ") + "反接" + "\n";
            }

        }
        else
        {
            detail = detail + MainWindow::tr("电池连接状态： ") + "已连接" + "\n";
        }

        // 显示电池充电状态
        if( bat_info_buff[idx].is_bat_chgring != true )
        {
            detail = detail + MainWindow::tr("电池充电状态： ") + "未充电" + "\n";
        }
        else
        {
            detail = detail + MainWindow::tr("电池充电状态： ") + "充电中" + "\n";
        }

        // 显示bms连接状态，如果未连接，直接显示未连接
        if( bat_info_buff[idx].is_bms_conn != true )
        {
            detail = detail + MainWindow::tr("BMS连接状态： ") + "未连接" + "\n";

            // 直接显示到界面
            QColor color = MainWindow::QTextDispTab[idx]->textColor();
            MainWindow::QTextDispTab[idx]->setTextColor(Qt::gray);
            MainWindow::QTextDispTab[idx]->setText(detail);
            MainWindow::QTextDispTab[idx]->setTextColor(color);

            // 清空对应的电池缓存信息
            memset( &bat_info_buff[idx], 0, sizeof(BatInfoDef) );
            continue;
        }
        else
            detail = detail + MainWindow::tr("BMS连接状态： ") + "已连接" + "\n";


        // 显示电池是否充满
        if( bat_info_buff[idx].is_bat_full == true )
        {
            detail = detail + MainWindow::tr("是否充满： ") + "已充满" + "\n";
        }
        else
            detail = detail + MainWindow::tr("是否充满： ") + "未充满" + "\n";

        detail = detail + MainWindow::tr("温度1： ") + QString::number( bat_info_buff[idx].tmp_1, 'f', 1 ) + MainWindow::tr("°") + "\n";
        detail = detail + MainWindow::tr("温度2： ") + QString::number( bat_info_buff[idx].tmp_2, 'f', 1 ) + MainWindow::tr("°") + "\n";
        detail = detail + MainWindow::tr("单体最高电压： ") + QString::number( bat_info_buff[idx].single_volt_max, 'f', 1 ) + MainWindow::tr(" V") + "\n";
        detail = detail + MainWindow::tr("单体最低电压： ") + QString::number( bat_info_buff[idx].single_volt_min, 'f', 1 ) + MainWindow::tr(" V") + "\n";
        detail = detail + MainWindow::tr("电池组电压： ") + QString::number( bat_info_buff[idx].bat_volt, 'f', 1 ) + MainWindow::tr(" V") + "\n";
        detail = detail + MainWindow::tr("电池组放电电流： ") + QString::number( bat_info_buff[idx].bat_discharge_current, 'f', 1 ) + MainWindow::tr(" A") + "\n";
        detail = detail + MainWindow::tr("电池组充电电流： ") + QString::number( bat_info_buff[idx].bat_chgr_current, 'f', 1 ) + MainWindow::tr(" A") + "\n";
        detail = detail + MainWindow::tr("剩余容量： ") + QString::number( bat_info_buff[idx].cap_remain, 'f', 1 ) + MainWindow::tr(" %") + "\n";
        detail = detail + MainWindow::tr("充电限流： ") + QString::number( bat_info_buff[idx].iout_limit, 'f', 1 ) + MainWindow::tr(" A") + "\n";
        detail = detail + MainWindow::tr("BMS告警字1： 0x") + QString::number( bat_info_buff[idx].warn_1, 16 ) + "\n";
        detail = detail + MainWindow::tr("BMS告警字2： 0x") + QString::number( bat_info_buff[idx].warn_2, 16 ) + "\n";
        detail = detail + MainWindow::tr("BMS告警字3： 0x") + QString::number( bat_info_buff[idx].warn_3, 16 ) + "\n";
        detail = detail + MainWindow::tr("BMS告警字4： 0x") + QString::number( bat_info_buff[idx].warn_4, 16 ) + "\n";

        // 显示
        QColor color = MainWindow::QTextDispTab[idx]->textColor();
        MainWindow::QTextDispTab[idx]->setTextColor(Qt::gray);
        MainWindow::QTextDispTab[idx]->setText(detail);
        MainWindow::QTextDispTab[idx]->setTextColor(color);

        // 清空对应的电池缓存信息
        memset( &bat_info_buff[idx], 0, sizeof(BatInfoDef) );
    }
}

/***********************************
 *
 * TCP client append a message
 * to message browser
 *
 ***********************************/
//void MainWindow::onTcpClientAppendMessage(const QString &from, const QString &message)
void MainWindow::onTcpClientAppendMessage(const QString &from, const QByteArray &message)
{
    if (from.isEmpty() || message.isEmpty())
    {
        return;
    }

    // 解析
    if( frame_parse(message) )
    {
        qDebug() << "frame parse success";
    }
    else
    {
        qDebug() << "frame parse failed";
        return;
    }

    // 显示
    disp_bat_info();
}

/***********************************
 *
 * Send message through TCP client
 *
 ***********************************/
void MainWindow::onTcpClientSendMessage()
{
#if 0
    QString text = ui->lineEdit_TcpClientSend->text();
    if (text.isEmpty())
    {
        return;
    }
#endif

    QString text("55AAA55A07000002040000FFFF52DD");
    mytcpclient->sendMessage(text);

//    mytcpclient->sendMessage(text);

//    QByteArray ba("Me");
//    onTcpClientAppendMessage(ba, text.toLatin1());
//    ui->lineEdit_TcpClientSend->clear();
}

/***********************************
 *
 * Setup connections
 *
 ***********************************/
bool MainWindow::setupConnection(quint8 type)
{
    bool isSuccess = false;
//    localAddr.setAddress(ui->label_LocalIP->text());

    switch (type)
    {
//    case TCPSERVER:
//        tcpServerListenPort = ui->lineEdit_TcpServerListenPort->text().toInt();
//        if (mytcpserver == nullptr)
//        {
//            mytcpserver = new MyTCPServer;
//        }
//        isSuccess = mytcpserver->listen(localAddr, tcpServerListenPort);
//        break;
    case TCPCLIENT:
        isSuccess = true;
        tcpClientTargetAddr.setAddress(ui->lineEdit_TcpClientTargetIP->text());
        tcpClientTargetPort = ui->lineEdit_TcpClientTargetPort->text().toInt();
        if (mytcpclient == nullptr)
        {
            mytcpclient = new MyTCPClient;
        }
        mytcpclient->connectTo(tcpClientTargetAddr, tcpClientTargetPort);
        break;
//    case UDPSERVER:
//        udpListenPort = ui->lineEdit_UdpListenPort->text().toInt();
//        isSuccess = myudp->bindPort(localAddr, udpListenPort);
//        break;
    }
    return isSuccess;
}

/***********************************
 *
 * Load settings from local configuration file
 *
 ***********************************/
void MainWindow::loadSettings()
{
    settingsFileDir = QApplication::applicationDirPath() + "/config.ini";
    QSettings settings(settingsFileDir, QSettings::IniFormat);
    ui->lineEdit_TcpClientTargetIP->setText(settings.value("TCP_CLIENT_TARGET_IP", "10.10.100.254").toString());
    ui->lineEdit_TcpClientTargetPort->setText(settings.value("TCP_CLIENT_TARGET_PORT", 8899).toString());
}


void MainWindow::saveSettings()
{
    QSettings settings(settingsFileDir, QSettings::IniFormat);
    settings.setValue("TCP_CLIENT_TARGET_IP", ui->lineEdit_TcpClientTargetIP->text());
    settings.setValue("TCP_CLIENT_TARGET_PORT", ui->lineEdit_TcpClientTargetPort->text());
    settings.sync();
}

// query timeout slot
void MainWindow::onTimeout_Query()
{
    qDebug("Query timeout");
}

// monitor timeout slot
void MainWindow::onTimeout_Monitor()
{
    qDebug("monitor time cnt timeout");

    if( m_monitor_time.sec > 59 )
    {
        m_monitor_time.sec = 0;
        m_monitor_time.min += 1;
        if( m_monitor_time.min > 59 )
        {
            m_monitor_time.min = 0;
            m_monitor_time.hour += 1;
        }
    }

    QString detail = "";
    detail = detail + QString::number(m_monitor_time.hour, 10) + MainWindow::tr("时");
    detail = detail + QString::number(m_monitor_time.min, 10) + MainWindow::tr("分");
    detail = detail + QString::number(m_monitor_time.sec, 10) + MainWindow::tr("秒");

    // 显示
    ui->lineEdit_MonitorTime->setAlignment(Qt::AlignRight);
    ui->lineEdit_MonitorTime->setText(detail);

    m_monitor_time.sec ++;
}

// disp current sys time timeout slot
void MainWindow::onTimeout_DispSysTime()
{
    QDateTime  date = QDateTime::currentDateTime();
    QString str( date.toString("yyyy-MM-dd hh:mm:ss ddd") );


//    QTime time = QTime::currentTime();
//    QString str( time.toString(Qt::ISODate) );

    ui->label_disp_sys_time->setText(str);
}

void MainWindow::on_pushButton_clicked()
{
    QMessageBox::about(this, tr("关于"), tr("AGV_HK充电站上位机监控测试程序\r\n功能： 监测充电站自动充电的充电信息\r\nAuthor: Liyang\r\nmail: liyang@ecthf.com"));
    return;
}
