/*
 * Copyright (C) 2013-2015 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mirbuffersgtexture.h"

// Mir
#include <mir/geometry/size.h>

// Qt
#include <QtGui/QOpenGLFunctions>

namespace mg = mir::geometry;

MirBufferSGTexture::MirBufferSGTexture()
    : QSGTexture()
    , m_width(0)
    , m_height(0)
    , m_textureId(0)
{
    auto f = QOpenGLContext::currentContext()->functions();
    f->glGenTextures(1, &m_textureId);

    setFiltering(QSGTexture::Linear);
    setHorizontalWrapMode(QSGTexture::ClampToEdge);
    setVerticalWrapMode(QSGTexture::ClampToEdge);
}

MirBufferSGTexture::~MirBufferSGTexture()
{
    if (m_textureId) {
        auto f = QOpenGLContext::currentContext()->functions();
        f->glDeleteTextures(1, &m_textureId);
    }
}

void MirBufferSGTexture::freeBuffer()
{
    m_mirBuffer.reset();
    m_width = 0;
    m_height = 0;
}

void MirBufferSGTexture::setBuffer(const std::shared_ptr<mir::graphics::Buffer>& buffer)
{
    m_mirBuffer.reset(buffer);
    mg::Size size = m_mirBuffer.size();
    m_height = size.height.as_int();
    m_width = size.width.as_int();
}

bool MirBufferSGTexture::hasBuffer() const
{
    return m_mirBuffer;
}

int MirBufferSGTexture::textureId() const
{
    return m_textureId;
}

QSize MirBufferSGTexture::textureSize() const
{
    return QSize(m_width, m_height);
}

bool MirBufferSGTexture::hasAlphaChannel() const
{
    return m_mirBuffer.has_alpha_channel();
}

void MirBufferSGTexture::bind()
{
    Q_ASSERT(hasBuffer());

    auto f = QOpenGLContext::currentContext()->functions();

    f->glBindTexture(GL_TEXTURE_2D, m_textureId);
    updateBindOptions(true/* force */);

    m_mirBuffer.bind_to_texture();
    m_mirBuffer.secure_for_render();

    // Fix for lp:1583088 - For non-GL clients, Mir uploads the client pixel buffer to a GL texture.
    // But as it does so, it changes some GL state and neglects to restore it, which breaks Qt's rendering.
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // 4 is the default which Qt uses
}
