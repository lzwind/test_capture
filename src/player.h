// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QWidget>
#include <QPointer>
#include <GLES2/gl2.h>
#include <EGL/egl.h>

class QOpenGLContext;
class QOpenGLTexture;
class TreelandCaptureContext;

class Player : public QWidget
{
    Q_OBJECT

public:
    explicit Player(QWidget *parent = nullptr);
    ~Player();

    TreelandCaptureContext *captureContext() const;
    void setCaptureContext(TreelandCaptureContext *context);

signals:
    void captureContextChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateTexture();
    void updateGeometry();
    void ensureDebugLogger();
    void initializeGL();

    QOpenGLContext *m_context{nullptr};
    QOpenGLTexture *m_texture{nullptr};
    QPointer<TreelandCaptureContext> m_captureContext;
    bool m_loggerInitialized{false};
    GLuint m_textureId{0};
};
