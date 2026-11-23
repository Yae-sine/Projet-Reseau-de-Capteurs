#include "mainwindow.h"
#include "ui_mainwindow.h"

//===============================================================
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    QWidget *zoneCentrale = new QWidget;
    setCentralWidget(zoneCentrale);

    but_send = new QPushButton("Send Message");
    m_layout = new QGridLayout();

    QLabel *label_to_send = new QLabel;
    label_to_send->setText("Message to Send");
    QLabel *label_received = new QLabel;
    label_received->setText("Message Received");

    send_box_0 = new QLineEdit();
    send_box_1 = new QLineEdit();
    send_box_2 = new QLineEdit();
    send_box_3 = new QLineEdit();
    send_box_4 = new QLineEdit();
    send_box_5 = new QLineEdit();
    send_box_6 = new QLineEdit();
    send_box_7 = new QLineEdit();

    receive_box_0 = new QLineEdit();
    receive_box_1 = new QLineEdit();
    receive_box_2 = new QLineEdit();
    receive_box_3 = new QLineEdit();
    receive_box_4 = new QLineEdit();
    receive_box_5 = new QLineEdit();
    receive_box_6 = new QLineEdit();
    receive_box_7 = new QLineEdit();

    m_layout->addWidget(label_to_send,0,1);

    m_layout->addWidget(but_send,1,0);

    m_layout->addWidget(send_box_7,1,1);
    m_layout->addWidget(send_box_6,1,2);
    m_layout->addWidget(send_box_5,1,3);
    m_layout->addWidget(send_box_4,1,4);
    m_layout->addWidget(send_box_3,1,5);
    m_layout->addWidget(send_box_2,1,6);
    m_layout->addWidget(send_box_1,1,7);
    m_layout->addWidget(send_box_0,1,8);

    m_layout->addWidget(label_received,2,1);

    m_layout->addWidget(receive_box_7,3,1);
    m_layout->addWidget(receive_box_6,3,2);
    m_layout->addWidget(receive_box_5,3,3);
    m_layout->addWidget(receive_box_4,3,4);
    m_layout->addWidget(receive_box_3,3,5);
    m_layout->addWidget(receive_box_2,3,6);
    m_layout->addWidget(receive_box_1,3,7);
    m_layout->addWidget(receive_box_0,3,8);

    zoneCentrale->setLayout(m_layout);

    openCANPort();

    timer_tick = new QTimer();
    connect( timer_tick, SIGNAL(timeout()), this, SLOT(onTimer_Tick()));


    connect(but_send,SIGNAL(clicked()),this,SLOT(onButSendClicked()));

     timer_tick -> start(1); // in ms
}
//===============================================================

MainWindow::~MainWindow()
{
    delete ui;
}

//===============================================================
void MainWindow::openCANPort()
{

    if (socket_can.open("can0") == scpp::STATUS_OK)
    {
         printf("can socket opened");
    }
    else
    {
        printf("Cannot open can socket!");
    }

}
//===============================================================
void MainWindow::onButSendClicked()
{
    sendCANMessage();
}
//===============================================================
void MainWindow::sendCANMessage()
{
    scpp::CanFrame frame_to_write;

    frame_to_write.id     =   0x4;
    frame_to_write.len     =   8;

    frame_to_write.data[0] =   send_box_0->text().toInt();
    frame_to_write.data[1] =   send_box_1->text().toInt();
    frame_to_write.data[2] =   send_box_2->text().toInt();
    frame_to_write.data[3] =   send_box_3->text().toInt();
    frame_to_write.data[4] =   send_box_4->text().toInt();
    frame_to_write.data[5] =   send_box_5->text().toInt();
    frame_to_write.data[6] =   send_box_6->text().toInt();
    frame_to_write.data[7] =   send_box_7->text().toInt();

    socket_can.write(frame_to_write);
}
//===============================================================
void MainWindow::receiveCANMessage()
{

 scpp::CanFrame fr;

    if(socket_can.read(fr) == scpp::STATUS_OK)
	{
		receive_box_0->setText(QString::number((uint)fr.data[0]));
		receive_box_1->setText(QString::number((uint)fr.data[1]));
		receive_box_2->setText(QString::number((uint)fr.data[2]));
		receive_box_3->setText(QString::number((uint)fr.data[3]));
		receive_box_4->setText(QString::number((uint)fr.data[4]));
		receive_box_5->setText(QString::number((uint)fr.data[5]));
		receive_box_6->setText(QString::number((uint)fr.data[6]));
		receive_box_7->setText(QString::number((uint)fr.data[7]));
	}

}
//===============================================================

void MainWindow::onTimer_Tick()
{
receiveCANMessage();
}


