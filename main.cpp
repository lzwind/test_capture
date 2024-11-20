// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "capture.h"

#include <private/qwaylandwindow_p.h>

#include <QDir>
#include <QGuiApplication>
#include <QOpenGLDebugLogger>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    QOpenGLDebugLogger glLogger;
    auto manager = TreelandCaptureManager::instance();
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    auto captureWithMask = [&app, manager, &glLogger](::wl_surface *mask) {
        auto captureContext = manager->ensureContext();
        if (!captureContext) {
            app.exit(-1);
            Q_UNREACHABLE();
        }
        captureContext->selectSource(TreelandCaptureContext::source_type_output
                                         | TreelandCaptureContext::source_type_window
                                         | TreelandCaptureContext::source_type_region,
                                     true,
                                     false,
                                     mask);

        QObject::connect(manager, &TreelandCaptureManager::finishSelect, manager, [&app, captureContext, manager] {
            if (manager->record()) {
                auto session = captureContext->ensureSession();
                session->start();
                // Q_EMIT manager->recordStartedChanged();
                QTimer::singleShot(1000, [manager] {
                    Q_EMIT manager->recordStartedChanged();
                });
            } else {
                auto frame = captureContext->ensureFrame();
                QImage result;
                QEventLoop loop;
                QObject::connect(frame,
                                 &TreelandCaptureFrame::ready,
                                 &app,
                                 [&result, &loop](QImage image) {
                                     result = image;
                                     loop.quit();
                                 });
                QObject::connect(frame, &TreelandCaptureFrame::failed, &app, [&loop] {
                    loop.quit();
                });
                loop.exec();
                if (result.isNull())
                    app.exit(-1);
                auto saveBasePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
                QDir saveBaseDir(saveBasePath);
                if (!saveBaseDir.exists())
                    app.exit(-1);
                QString picName =
                    "portal screenshot - " + QDateTime::currentDateTime().toString() + ".png";
                if (result.save(saveBaseDir.absoluteFilePath(picName), "PNG")) {
                    qDebug() << saveBaseDir.absoluteFilePath(picName);
                } else {
                    app.exit(-1);
                }
            }
        });
    };
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        manager,
        [&app, manager, captureWithMask](QObject *object, const QUrl &url) {
            if (auto canvasWindow = qobject_cast<QQuickWindow *>(object)) {
                auto waylandWindow =
                    static_cast<QtWaylandClient::QWaylandWindow *>(canvasWindow->handle());
                if (manager->isActive()) {
                    captureWithMask(waylandWindow->surface());
                } else {
                    QObject::connect(manager,
                                     &TreelandCaptureManager::activeChanged,
                                     manager,
                                     std::bind(captureWithMask, waylandWindow->surface()));
                }
            }
        });
    engine.loadFromModule("capture", "Main");
    return app.exec();
}