#include "mainwindow.h"
#include "subwindow.h"
#include "capture.h"
#include "player.h"

#include <private/qwaylandwindow_p.h>
#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandintegration_p.h>

#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QApplication>
#include <QWindow>
#include "subwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput);

    
    setupUI();
    
    // 初始化截图/录屏管理器
    auto manager = TreelandCaptureManager::instance();
    if (manager->isActive()) {
        initializeCapture();
    } else {
        connect(manager, &TreelandCaptureManager::activeChanged, 
                this, &MainWindow::initializeCapture);
    }
    
    resize(600, 400);
}

MainWindow::~MainWindow()
{
    delete m_toolBar;
    delete m_player;
}

void MainWindow::setupUI()
{
    auto centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // 创建水印标签
    m_watermark = new QLabel(this);
    m_watermark->setPixmap(QPixmap(":/watermark.png"));
    m_watermark->hide();

    // 创建播放器窗口
    m_player = new Player();
    m_player->hide();
}

void MainWindow::setupConnections()
{
    connect(m_watermarkBtn, &QPushButton::clicked, 
            this, &MainWindow::onWatermarkToggled);
    connect(m_recordBtn, &QPushButton::clicked, 
            this, &MainWindow::onRecordToggled);
    connect(m_finishBtn, &QPushButton::clicked, 
            this, &MainWindow::onFinishClicked);
}

void MainWindow::initializeCapture()
{
    auto manager = TreelandCaptureManager::instance();
    auto captureContext = manager->ensureContext();
    if (!captureContext) {
        qApp->exit(-1);
        return;
    }



    auto waylandWindow = static_cast<QtWaylandClient::QWaylandWindow *>(windowHandle()->handle());

    connect(manager->context(), &TreelandCaptureContext::captureRegionChanged,
            this, &MainWindow::updateCaptureRegion);

    connect(manager, &TreelandCaptureManager::recordStartedChanged,
            this, &MainWindow::slotRecoderShow);
    captureContext->selectSource(
        TreelandCaptureContext::source_type_output
        | TreelandCaptureContext::source_type_window
        | TreelandCaptureContext::source_type_region,
        true,
        false,
        waylandWindow->surface()
    );

    connect(manager, &TreelandCaptureManager::finishSelect,
            this, &MainWindow::handleCaptureFinish);
}

void MainWindow::handleCaptureFinish()
{
    auto manager = TreelandCaptureManager::instance();
    auto captureContext = manager->context();
    
    if (manager->record()) {
        // 录屏模式
        auto session = captureContext->ensureSession();
        session->start();
        m_player->setCaptureContext(captureContext);
        QTimer::singleShot(1000, [manager] {
            Q_EMIT manager->recordStartedChanged();
        });
    } else {
        // 截图模式
        auto frame = captureContext->ensureFrame();
        QImage result;
        QEventLoop loop;
        
        connect(frame, &TreelandCaptureFrame::ready,
                this, [&result, &loop](QImage image) {
                    result = image;
                    loop.quit();
                });
                
        connect(frame, &TreelandCaptureFrame::failed,
                this, [&loop] {
                    loop.quit();
                });
                
        loop.exec();
        
        if (result.isNull()) {
            qApp->exit(-1);
            return;
        }
        
        // 保存截图
        auto saveBasePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        QDir saveBaseDir(saveBasePath);
        if (!saveBaseDir.exists()) {
            qApp->exit(-1);
            return;
        }
        
        QString picName = "portal screenshot - " + 
                         QDateTime::currentDateTime().toString() + 
                         ".png";
                         
        if (result.save(saveBaseDir.absoluteFilePath(picName), "PNG")) {
            qDebug() << "Saved to:" << saveBaseDir.absoluteFilePath(picName);
        } else {
            qApp->exit(-1);
        }
    }
}

void MainWindow::slotRecoderShow()
{
    if(m_player)
    {
        m_player->show();
    }
}

void MainWindow::onWatermarkToggled()
{
    m_watermarkVisible = !m_watermarkVisible;
    m_watermark->setVisible(m_watermarkVisible);
    m_watermarkBtn->setText(m_watermarkVisible ? "Hide watermark" : "Show watermark");
}

void MainWindow::onRecordToggled()
{
    auto manager = TreelandCaptureManager::instance();
    manager->setRecord(!manager->record());
    m_recordBtn->setText(manager->record() ? "Screenshot" : "Record");
}

void MainWindow::onFinishClicked()
{
    TreelandCaptureManager::instance()->finishSelect();
}

void MainWindow::updateCaptureRegion()
{
    auto context = TreelandCaptureManager::instance()->context();
    if (!context) return;
    
    QRect region = context->captureRegion().toRect();
    m_watermark->setGeometry(region);


    if(!m_toolBar)
    {
        // 创建工具栏
        m_toolBar = new SubWindow(this);

        auto toolBarLayout = new QHBoxLayout(m_toolBar);

        m_watermarkBtn = new QPushButton("Show watermark", m_toolBar);
        m_recordBtn = new QPushButton("Record", m_toolBar);
        m_finishBtn = new QPushButton("Finish", m_toolBar);

        toolBarLayout->addWidget(m_watermarkBtn);
        toolBarLayout->addWidget(m_recordBtn);
        toolBarLayout->addWidget(m_finishBtn);

        m_toolBar->setLayout(toolBarLayout);
          setupConnections();
    }
    
    // 更新工具栏位置
    m_toolBar->move(region.x(),
                   qMin(region.bottom(), height() - 2 * m_toolBar->height()));

    m_toolBar->show();
    if(windowHandle()&&m_toolBar->windowHandle())
    {
        if(m_toolBar->windowHandle()->parent() != windowHandle())
        {
            m_toolBar->windowHandle()->setParent(windowHandle());
            m_toolBar->show();
        }
    }

}
