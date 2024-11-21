// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "player.h"
#include "capture.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <libdrm/drm_fourcc.h>

#include <QPainter>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <unistd.h>
#include <sys/mman.h>

Player::Player(QWidget *parent)
    : QWidget(parent)
{
    // 设置窗口属性
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    // 设置 OpenGL 格式
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(2, 0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    // 创建 OpenGL 上下文
    m_context = new QOpenGLContext(this);
    m_context->setFormat(format);
    if (!m_context->create()) {
        qWarning() << "Failed to create OpenGL context";
        return;
    }

    // 确保创建原生窗口
    winId();

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    resize(400, 300);
}

Player::~Player()
{
    if (m_textureId) {
        glDeleteTextures(1, &m_textureId);
    }
}

TreelandCaptureContext *Player::captureContext() const
{
    return m_captureContext;
}

void Player::setCaptureContext(TreelandCaptureContext *context)
{
    if (context == m_captureContext)
        return;

    if (m_captureContext) {
        m_captureContext->disconnect(this);
    }

    m_captureContext = context;

    if (m_captureContext) {
        connect(m_captureContext.data(),
                &TreelandCaptureContext::destroyed,
                this,
                std::bind(&Player::setCaptureContext, this, nullptr));

        if (m_captureContext->session()) {
            connect(m_captureContext->session(),
                    &TreelandCaptureSession::ready,
                    this,
                    [this]() { update(); });
        }

        connect(m_captureContext.data(),
                &TreelandCaptureContext::sourceReady,
                this,
                &Player::updateGeometry);
    }

    emit captureContextChanged();
}

void Player::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    if (!m_captureContext || !m_captureContext->session() || !m_captureContext->session()->started())
        return;

    if (!windowHandle()) {
        qWarning() << "No window handle available for OpenGL rendering";
        return;
    }
        updateTexture();
    
    if (!QOpenGLContext::currentContext()) {
        qWarning() << "Failed to make OpenGL context current";
        return;
    }




    QPainter painter(this);
    painter.beginNativePainting();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 设置视口
    glViewport(0, 0, width(), height());

    // 绘制纹理
    if (m_textureId) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_textureId);

        // 使用 OpenGL ES 2.0 兼容的方式绘制四边形
        const GLfloat vertices[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f,
            -1.0f,  1.0f
        };

        const GLfloat texCoords[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f
        };

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        
        glVertexPointer(2, GL_FLOAT, 0, vertices);
        glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
        
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

    painter.endNativePainting();
    
    m_context->swapBuffers(windowHandle());
}

void Player::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void Player::updateTexture()
{
    ensureDebugLogger();

    auto session = m_captureContext->session();
    if (!session) return;

     auto objects = session->objects();
    if (objects.empty()) return;

    // 获取第一个平面的 fd 和尺寸信息
    int dma_fd = objects[0].fd;
    int width = session->bufferWidth();
    int height = session->bufferHeight();
    int stride = objects[0].stride;

    // 映射 DMA-BUF 内存
    unsigned char *mapData = static_cast<unsigned char *>(mmap(nullptr, 
                                                             stride * height, 
                                                             PROT_READ, 
                                                             MAP_SHARED, 
                                                             dma_fd, 
                                                             0));
    
    if (MAP_FAILED == mapData) {
        qWarning() << "DMA-BUF mmap failed for fd:" << dma_fd 
                  << "Error:" << strerror(errno);
        return;
    }

    // 创建 QImage 并保存副本
    {
        QImage currentImage = QImage(mapData,
                              width, 
                              height, 
                              QImage::Format_RGBA8888).copy();
        currentImage.save("/home/uos/test.png");
    }

    // 解除内存映射
    munmap(mapData, stride * height);




    // EGLAttrib attribs[47];
    // int atti = 0;
    // attribs[atti++] = EGL_WIDTH;
    // attribs[atti++] = session->bufferWidth();
    // attribs[atti++] = EGL_HEIGHT;
    // attribs[atti++] = session->bufferHeight();
    // attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
    // attribs[atti++] = session->bufferFormat();

    // auto objects = session->objects();
    // auto nPlanes = objects.size();

    // if (nPlanes > 0) {
    //     attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
    //     attribs[atti++] = objects[0].fd;
    //     attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    //     attribs[atti++] = objects[0].offset;
    //     attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    //     attribs[atti++] = objects[0].stride;
    //     if (m_captureContext->session()->modifierUnion().modifier) {
    //         attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
    //         attribs[atti++] = m_captureContext->session()->modifierUnion().modLow;
    //         attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
    //         attribs[atti++] = m_captureContext->session()->modifierUnion().modHigh;
    //     }
    // }

    // if (nPlanes > 1) {
    //     attribs[atti++] = EGL_DMA_BUF_PLANE1_FD_EXT;
    //     attribs[atti++] = objects[1].fd;
    //     attribs[atti++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
    //     attribs[atti++] = objects[1].offset;
    //     attribs[atti++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
    //     attribs[atti++] = objects[1].stride;
    //     if (m_captureContext->session()->modifierUnion().modifier) {
    //         attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
    //         attribs[atti++] = m_captureContext->session()->modifierUnion().modLow;
    //         attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
    //         attribs[atti++] = m_captureContext->session()->modifierUnion().modHigh;
    //     }
    // }

    // if (nPlanes > 2) {
    //     attribs[atti++] = EGL_DMA_BUF_PLANE2_FD_EXT;
    //     attribs[atti++] = objects[2].fd;
    //     attribs[atti++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
    //     attribs[atti++] = objects[2].offset;
    //     attribs[atti++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
    //     attribs[atti++] = objects[2].stride;
    //     if (m_captureContext->session()->modifierUnion().modifier) {
    //         attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
    //         attribs[atti++] = m_captureContext->session()->modifierUnion().modLow;
    //         attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
    //         attribs[atti++] = m_captureContext->session()->modifierUnion().modHigh;
    //     }
    // }

    // if (nPlanes > 3) {
    //     attribs[atti++] = EGL_DMA_BUF_PLANE3_FD_EXT;
    //     attribs[atti++] = objects[3].fd;
    //     attribs[atti++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
    //     attribs[atti++] = objects[3].offset;
    //     attribs[atti++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
    //     attribs[atti++] = objects[3].stride;
    //     if (m_captureContext->session()->modifierUnion().modifier) {
    //         attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
    //         attribs[atti++] = m_captureContext->session()->modifierUnion().modLow;
    //         attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
    //         attribs[atti++] = m_captureContext->session()->modifierUnion().modHigh;
    //     }
    // }

    // attribs[atti++] = EGL_NONE;

    // auto eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    // EGLImage eglImage = eglCreateImage(eglDisplay, 
    //                                  EGL_NO_CONTEXT,
    //                                  EGL_LINUX_DMA_BUF_EXT,
    //                                  nullptr,
    //                                  attribs);

    // if (eglImage == EGL_NO_IMAGE) {
    //   qWarning() << "Failed to create EGL image. Error code:" << eglGetError();
    //   return;
    // }

    // // 创建或更新纹理
    // if (!m_textureId) {
    //     glGenTextures(1, &m_textureId);
    // }

    // glBindTexture(GL_TEXTURE_2D, m_textureId);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
    // glBindTexture(GL_TEXTURE_2D, 0);

    // // 清理资源
    // eglDestroyImage(eglDisplay, eglImage);
    for (const auto &object : objects) {
        if (object.fd > 0)
            ::close(object.fd);
    }
}

void Player::updateGeometry()
{
    if (!m_captureContext) return;
    
    QRect region = m_captureContext->captureRegion().toRect();
    resize(region.width(), region.height());
}

static void EGLAPIENTRY debugCallback(EGLenum error,
                                      [[maybe_unused]] const char *command,
                                      [[maybe_unused]] EGLint messageType,
                                      [[maybe_unused]] EGLLabelKHR threadLabel,
                                      [[maybe_unused]] EGLLabelKHR objectLabel,
                                      const char *message)
{
    qInfo() << "EGL Debug:" << message << "(Error Code:" << error << ")";
}

void Player::ensureDebugLogger()
{
    if (m_loggerInitialized)
        return;
    if(!QOpenGLContext::currentContext()) return;
    if (QOpenGLContext::currentContext()->hasExtension(QByteArrayLiteral("GL_KHR_debug"))) {
        const EGLAttrib debugAttribs[] = {
            EGL_DEBUG_MSG_CRITICAL_KHR,
            EGL_TRUE,
            EGL_DEBUG_MSG_ERROR_KHR,
            EGL_TRUE,
            EGL_DEBUG_MSG_WARN_KHR,
            EGL_TRUE,
            EGL_DEBUG_MSG_INFO_KHR,
            EGL_TRUE,
            EGL_NONE,
        };
        PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR =
            (PFNEGLDEBUGMESSAGECONTROLKHRPROC)eglGetProcAddress("eglDebugMessageControlKHR");
        if (eglDebugMessageControlKHR) {
            eglDebugMessageControlKHR(debugCallback, debugAttribs);
        } else {
            qCritical() << "Failed to get eglDebugMessageControlKHR function.";
        }
    } else {
        qCritical() << "GL_KHR_debug is not supported.";
    }
    m_loggerInitialized = true;
}
