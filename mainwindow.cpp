#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QImage>
#include <QImageReader>
//#include "ImgProcessor.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
	listener(controller, this)
{
    ui->setupUi(this);
	setWindowTitle("Leap Manipulator");
    initServerCamera ();
    initProcess ();
	m_widgetSpinner = nullptr;
	m_pControlPanel = nullptr;
	m_pSerial = nullptr;
	m_pWidgetSpinner = nullptr;
	m_pCloseSerial = nullptr;

//	ui->actionControlPanel->setEnabled(false);
//	ui->actionSpinner->setEnabled(false);
}

MainWindow::~MainWindow()
{
	//listener.quit();
	delete m_pWidgetSpinner;
	delete m_pSerial;
	delete m_pControlPanel;
	m_process->close();
	delete m_process;
	m_processVApp->close();
	delete m_processVApp;
    delete m_widgetSpinner;
    delete ui;
}

void MainWindow::createWidgetSpinner ()
{
	if (m_pWidgetSpinner == nullptr)
		m_pWidgetSpinner = new WidgetSpinner(&listener);
	m_pWidgetSpinner->show();
	m_pWidgetSpinner->raise();
	
}

QFrame *MainWindow::createSpinner ()
{//discard
    QFrame *frame = new QFrame();
    QVBoxLayout *layout = new QVBoxLayout();
    QDial *dial = new QDial(frame);
    dial->setNotchesVisible (true);
    dial->setRange (0,180);
    QLCDNumber *lcd = new QLCDNumber(frame);
    lcd->setDigitCount (3);
    lcd->display (111);
    lcd->setSegmentStyle (QLCDNumber::Flat);
    layout->addWidget (dial);
    layout->addWidget (lcd);
    frame->setLayout (layout);
   // connect (dial,&QDial::valueChanged,lcd,);
    return frame;
}

void MainWindow::createDockSpinner ()
{//discard
    m_dock = new QDockWidget(tr("Servo Spinner"), this);
    QFrame *frame = new  QFrame(m_dock);
    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    QVBoxLayout *vLayout = new QVBoxLayout;
    ServoSpinner **spinners = new ServoSpinner*[NUMSERVO];
    for(int i = 0; i<NUMSERVO; i++){
        spinners[i] = new ServoSpinner(frame, 0, 180, 123, i);
        vLayout->addWidget (spinners[i]);
    }
    frame->setLayout (vLayout);
    m_dock->setWidget (frame);

    addDockWidget(Qt::RightDockWidgetArea, m_dock);
}

void MainWindow::initServerCamera ()
{
    tcpServer = new QTcpServer(this);
    m_numBytesRecved = 0;
    m_nImageSize = 0;
    if(!tcpServer->listen(QHostAddress::Any, 8002))
    {
        qDebug()<<tcpServer->errorString();
        close();
    }
    connect (tcpServer, &QTcpServer::newConnection, this, &MainWindow::acceptConnection);
	ui->statusBar->showMessage(tr("camera initialized. wait to connect to camera..."));
}

void MainWindow::initProcess ()
{//init QProcess for SSH
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyRead, this, &MainWindow::showProcessResult);
    connect(m_process, &QProcess::stateChanged,
            this, &MainWindow::showProcessState);
    connect(m_process, &QProcess::errorOccurred,
            this, &MainWindow::showProcessError);
    connect(m_process, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(showProcessFinished(int, QProcess::ExitStatus)));
	m_processVApp = new QProcess(this);
	m_processSer = new QProcess(this);
}

void MainWindow::acceptConnection ()
{
    qDebug()<<"accept new connection";
    ui->statusBar->showMessage (tr("accept new connection"), 2000);
    tcpServerConnection = tcpServer->nextPendingConnection ();
    connect(tcpServerConnection, SIGNAL(readyRead()),this, SLOT(receiveImage()));
    connect(tcpServerConnection, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(displaySocketError(QAbstractSocket::SocketError)));
	m_pImgProc = new ImgProcessor(this);
	connect(m_pImgProc, &ImgProcessor::sig_trans, &listener, &FrameListener::handleTrans);
}

void MainWindow::displayImage ()
{
    QByteArray data = tcpServerConnection->read (m_nImageSize);
    QBuffer buf(&data);
    buf.open (QIODevice::ReadOnly);

    QImageReader reader(&buf, "jpg");
    QImage image = reader.read ();
	
	QImage nImg = m_pImgProc->processImg(image);

    ui->label_display->setPixmap (QPixmap::fromImage (nImg));

    m_nImageSize = 0;
}

void MainWindow::receiveImage ()
{
    if(m_nImageSize==0){
        if(tcpServerConnection->bytesAvailable () < HEADSIZE)
            return;
        QByteArray buf = tcpServerConnection->read (HEADSIZE);
     //   qDebug()<<"buf: "<<buf;
        m_nImageSize = buf.toUInt ();
    //    qDebug()<<"buf.toint() :"<<m_nImageSize;
    }

    if(tcpServerConnection->bytesAvailable () < m_nImageSize){
        return;
    }
    displayImage ();
}

void MainWindow::readySend ()
{
	QMessageBox::information(this, "Information", "SerialWIFI Ready");
	ui->statusBar->showMessage("SerialWIFI Connected..", 2000);
	if (((AS_6DOF*)as6dof)->initModel() < 0) {
		QMessageBox::critical(this, "Critical", "Failed to Open Remote Serial.");
		m_pSerial = nullptr;
	}
	else 
	{
		ui->actionTo_Serial->setEnabled(false);
		ui->actionTo_Servo_Controller->setEnabled(false);
		setWindowTitle("Leap Manipulator - [SerialWIFI Conneted]");
		if (m_pControlPanel == nullptr)
		{
			m_pControlPanel = new ControlPanel(&listener, 0);
			m_pControlPanel->setGeometry(50, 50, 400, 500);
		}
		m_pControlPanel->show();
	}
}

void MainWindow::on_actionAbout_triggered() {
	QMessageBox::about(this, "About", "JLU");
}
void MainWindow::on_actionTo_Camera_triggered()
{
//    m_process->start ("putty.exe", QStringList()<<"-pw"<<"raspberry"<<"-m"
//                      <<"camera.sh"<<"pi@192.168.137.117");
    m_process->start ("putty.exe -pw raspberry -m camera.sh pi@192.168.137.210");
	//m_process->close();
}
void MainWindow::on_actionVisualizer_triggered()
{
	m_processVApp->start("VisualizerApp.exe");
	
}
void MainWindow::on_actionTo_Serial_triggered()
{
	if (m_pSerial == nullptr) {
		m_pSerial = new SerialWin("\\\\.\\COM5");
		as6dof = factory.createModel(ModelFactory::AS6DOF);
		as6dof->setSerial(m_pSerial);
		listener.addObserverModel(as6dof);
	}
	if (m_pSerial!=nullptr && m_pSerial->IsConnected()) {
		qDebug() << "Serial Opened.." << endl;
		QMessageBox::information(this, "Information", "SerialWin Ready");
		ui->statusBar->showMessage("Serial Opened..", 2000);
		ui->actionTo_Serial->setEnabled(false);
		ui->actionTo_Servo_Controller->setEnabled(false);
		setWindowTitle("Leap Manipulator - [SerialWin Conneted]");
	}
	else
	{
		QMessageBox::critical(this, "Critical", "Failed to Open Serial.");
		qDebug() << "Failed to Open Serial." << endl;
		m_pSerial = nullptr;
		return;
	}
		
	if (m_pControlPanel==nullptr)
	{
		m_pControlPanel = new ControlPanel(&listener, 0);
		m_pControlPanel->setGeometry(50, 50, 400, 500);
	}
	m_pControlPanel->show();
	//m_pCloseSerial = ui->mainToolBar->addAction("close serial");
	//connect(m_pCloseSerial, &QAction::triggered, this, &MainWindow::closeSerial);
}

void MainWindow::on_actionTo_Servo_Controller_triggered()
{
	if (m_pSerial == nullptr) {
		m_pSerial = new SerialWIFI(this); 
		m_processSer->start("putty.exe -pw raspberry -m servo.sh pi@192.168.137.210");
		//ModelFactory factory;
		as6dof = factory.createModel(ModelFactory::AS6DOF);
		as6dof->setSerial(m_pSerial);
		listener.addObserverModel(as6dof);
		connect((SerialWIFI*)m_pSerial, &SerialWIFI::newConnection, this, &MainWindow::readySend);
	}
	else {
		QMessageBox::information(this, "Information", "Serial already Opened!");
	}
//	ui->mainToolBar->addAction("close serial");
//	m_pCloseSerial = ui->mainToolBar->addAction("close serial");
//	connect(m_pCloseSerial, &QAction::triggered, this, &MainWindow::closeSerial);
}

void MainWindow::on_actionControlPanel_triggered()
{
	if (m_pSerial == nullptr) {
		QMessageBox::critical(this, "Critical", "Serial Not Initialized!");
		return;
	}
	if (!m_pSerial->IsConnected()) {
		QMessageBox::critical(this, "Critical", "Serial Not Connected yet!");
		return;
	}
	if (m_pControlPanel == nullptr)
	{
		m_pControlPanel = new ControlPanel(&listener, 0);
		m_pControlPanel->setGeometry(50, 50, 400, 500);
	}
	m_pControlPanel->show();
}


void MainWindow::showProcessResult()
{
    QTextCodec *codec = QTextCodec::codecForLocale();
    qDebug() << "showResult: " << endl << codec->toUnicode(m_process->readAll());
}

void MainWindow::showProcessState(QProcess::ProcessState state)
{
    qDebug() << "showState: ";
    if (state == QProcess::NotRunning) {
        qDebug() << "Not Running";
    } else if (state == QProcess::Starting) {
        qDebug() << "Starting";
    }  else {
        qDebug() << "Running";
	//	m_process->close();
    }
}

void MainWindow::showProcessError()
{
    qDebug() << "showError: " << endl << m_process->errorString();
}

void MainWindow::showProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "showFinished: " << endl << exitCode << exitStatus;
    QString str = QString("process finished. exitCode: %1, exitStatus: %2.").arg (exitCode).arg(exitStatus);
    ui->statusBar->showMessage (str, 2000);
}
void MainWindow::closeSerial()
{
	delete m_pSerial;
	m_pSerial = nullptr;
	m_pCloseSerial->setVisible(false);
	ui->actionTo_Serial->setEnabled(true);
	ui->actionTo_Servo_Controller->setEnabled(true);
	m_pControlPanel->hide();
//	m_widgetSpinner->hide();
}
void MainWindow::displaySocketError (QAbstractSocket::SocketError socketError)
{
    qDebug() << tcpServerConnection->errorString();
    tcpServerConnection->close();
}

void MainWindow::on_actionSpinner_triggered()
{
    createWidgetSpinner ();
}
