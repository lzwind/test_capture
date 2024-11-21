#pragma once

#include <QMainWindow>
#include "capture.h"

class SubWindow;
class QLabel;
class QPushButton;
class Player;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onWatermarkToggled();
    void onRecordToggled();
    void onFinishClicked();
    void updateCaptureRegion();
    void initializeCapture();
    void handleCaptureFinish();

    void slotRecoderShow();

private:
    void setupUI();
    void setupConnections();

    SubWindow *m_toolBar = nullptr;
    QLabel *m_watermark;
    QPushButton *m_watermarkBtn;
    QPushButton *m_recordBtn;
    QPushButton *m_finishBtn;
    Player *m_player;
    bool m_watermarkVisible{false};
};
