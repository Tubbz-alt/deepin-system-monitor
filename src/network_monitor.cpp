/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2011 ~ 2018 Deepin, Inc.
 *               2011 ~ 2018 Wang Yong
 *
 * Author:     Wang Yong <wangyong@deepin.com>
 * Maintainer: Wang Yong <wangyong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <DApplication>
#include <DApplicationHelper>
#include <DHiDPIHelper>
#include <DPalette>
#include <DStyle>
#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QtMath>

#include "constant.h"
#include "network_monitor.h"
#include "process/system_monitor.h"
#include "smooth_curve_generator.h"
#include "utils.h"

DWIDGET_USE_NAMESPACE

using namespace Utils;

NetworkMonitor::NetworkMonitor(QWidget *parent)
    : QWidget(parent)
{
    DStyle *style = dynamic_cast<DStyle *>(DApplication::style());
    auto *dAppHelper = DApplicationHelper::instance();
    QStyleOption option;
    option.initFrom(this);
    int margin = style->pixelMetric(DStyle::PM_ContentsMargins, &option);
    m_margin = margin;

    int statusBarMaxWidth = Utils::getStatusBarMaxWidth();
    setFixedWidth(statusBarMaxWidth - margin * 2);
    setFixedHeight(180);

    pointsNumber = int(statusBarMaxWidth / 5.4);

    downloadSpeeds = new QList<double>();
    for (int i = 0; i < pointsNumber; i++) {
        downloadSpeeds->append(0);
    }

    uploadSpeeds = new QList<double>();
    for (int i = 0; i < pointsNumber; i++) {
        uploadSpeeds->append(0);
    }

    connect(dAppHelper, &DApplicationHelper::themeTypeChanged, this, &NetworkMonitor::changeTheme);
    changeTheme(dAppHelper->themeType());

    auto *sysmon = SystemMonitor::instance();
    if (sysmon) {
        connect(sysmon, &SystemMonitor::networkStatInfoUpdated, this,
                &NetworkMonitor::updateStatus);
    }

    changeFont(DApplication::font());
    connect(dynamic_cast<QGuiApplication *>(DApplication::instance()), &DApplication::fontChanged,
            this, &NetworkMonitor::changeFont);
}

NetworkMonitor::~NetworkMonitor()
{
    delete downloadSpeeds;
    delete uploadSpeeds;
}

void NetworkMonitor::changeTheme(DApplicationHelper::ColorType themeType)
{
    switch (themeType) {
        case DApplicationHelper::LightType:
            iconImage = QIcon(":/image/light/icon_network_light.svg").pixmap(QSize(24, 24));
            break;
        case DApplicationHelper::DarkType:
            iconImage = QIcon(":/image/dark/icon_network_light.svg").pixmap(QSize(24, 24));
            break;
        default:
            break;
    }

    // init colors
    auto *dAppHelper = DApplicationHelper::instance();
    auto palette = dAppHelper->applicationPalette();
    textColor = palette.color(DPalette::Text);
    summaryColor = palette.color(DPalette::TextTips);
    //    m_frameColor = palette.color(DPalette::FrameBorder);
    m_frameColor = "#10FFFFFF";
}

void NetworkMonitor::updateStatus(long tRecvBytes, long tSentBytes, float tRecvKbs, float tSentKbs)
{
    totalRecvBytes = tRecvBytes;
    totalSentBytes = tSentBytes;
    totalRecvKbs = tRecvKbs;
    totalSentKbs = tSentKbs;

    // Init download path.
    downloadSpeeds->append(totalRecvKbs);

    if (downloadSpeeds->size() > pointsNumber) {
        downloadSpeeds->pop_front();
    }

    QList<QPointF> downloadPoints;

    double downloadMaxHeight = 0;
    for (int i = 0; i < downloadSpeeds->size(); i++) {
        if (downloadSpeeds->at(i) > downloadMaxHeight) {
            downloadMaxHeight = downloadSpeeds->at(i);
        }
    }

    for (int i = 0; i < downloadSpeeds->size(); i++) {
        if (downloadMaxHeight < downloadRenderMaxHeight) {
            downloadPoints.append(QPointF(i * 5, downloadSpeeds->at(i)));
        } else {
            downloadPoints.append(QPointF(
                i * 5, downloadSpeeds->at(i) * downloadRenderMaxHeight / downloadMaxHeight));
        }
    }

    downloadPath = SmoothCurveGenerator::generateSmoothCurve(downloadPoints);

    // Init upload path.
    uploadSpeeds->append(totalSentKbs);

    if (uploadSpeeds->size() > pointsNumber) {
        uploadSpeeds->pop_front();
    }

    QList<QPointF> uploadPoints;

    double uploadMaxHeight = 0;
    for (int i = 0; i < uploadSpeeds->size(); i++) {
        if (uploadSpeeds->at(i) > uploadMaxHeight) {
            uploadMaxHeight = uploadSpeeds->at(i);
        }
    }

    for (int i = 0; i < uploadSpeeds->size(); i++) {
        if (uploadMaxHeight < uploadRenderMaxHeight) {
            uploadPoints.append(QPointF(i * 5, uploadSpeeds->at(i)));
        } else {
            uploadPoints.append(
                QPointF(i * 5, uploadSpeeds->at(i) * uploadRenderMaxHeight / uploadMaxHeight));
        }
    }

    uploadPath = SmoothCurveGenerator::generateSmoothCurve(uploadPoints);

    repaint();
}

void NetworkMonitor::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    //    painter.fillRect(rect(), "#10f1ab35");

    int spacing = 6;
    int sectionSize = 6;

    QFontMetrics fmSection(m_sectionFont);
    QString sectionTitle = DApplication::translate("Process.Graph.View", "Network");

    // Draw title.
    QRect titleRect(rect().x() + iconImage.width() + 4, rect().y(), fmSection.width(sectionTitle),
                    fmSection.height());

    painter.setFont(m_sectionFont);
    painter.setPen(QPen(textColor));
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, sectionTitle);

    // Draw icon.
    QRect iconRect(rect().x(),
                   titleRect.y() + qCeil((titleRect.height() - iconImage.height()) / 2.),
                   iconImage.width(), iconImage.height());
    painter.drawPixmap(iconRect, iconImage);

    // Draw network summary.
    QString recvTitle = DApplication::translate("Process.Graph.View", "Download");
    QString recvContent = formatBandwidth(totalRecvKbs);
    QString recvTotalTitle = DApplication::translate("Process.Graph.View", "Total Received");
    QString recvTotalContent = formatByteCount(totalRecvBytes);

    QString sentTitle = DApplication::translate("Process.Graph.View", "Upload");
    QString sentContent = formatBandwidth(totalSentKbs);
    QString sentTotalTitle = DApplication::translate("Process.Graph.View", "Total Sent");
    QString sentTotalContent = formatByteCount(totalSentBytes);

    QFontMetrics fmContent(m_contentFont);
    QFontMetrics fmSubContent(m_subContentFont);
    int cw1 = std::max(fmContent.width(recvTitle), fmContent.width(sentTitle));
    int cw2 = std::max(fmContent.width(recvContent), fmContent.width(sentContent));
    int cw3 = std::max(fmContent.width(recvTotalTitle), fmContent.width(recvTotalTitle));
    int cw4 = std::max(fmContent.width(recvTotalContent), fmContent.width(sentTotalContent));
    QRect crect11(sectionSize * 2 + 4, titleRect.y() + titleRect.height() + spacing, cw1,
                  fmContent.height());
    QRect crect12(crect11.x() + cw1 + 4, crect11.y(), cw2, crect11.height());
    QRect crect14(rect().width() - cw4, crect11.y(), cw4, crect11.height());
    QRect crect13(crect14.x() - 4 - cw3, crect11.y(), cw3, crect11.height());
    QRect crect21(crect11.x(), crect11.y() + crect11.height(), cw1, crect11.height());
    QRect crect22(crect12.x(), crect21.y(), cw2, crect21.height());
    QRect crect24(crect14.x(), crect21.y(), cw4, crect21.height());
    QRect crect23(crect13.x(), crect21.y(), cw3, crect21.height());
    QRectF r1Ind(3, crect11.y() + qCeil((crect11.height() - sectionSize) / 2.), sectionSize,
                 sectionSize);
    QRectF r2Ind(3, crect21.y() + qCeil((crect21.height() - sectionSize) / 2.), sectionSize,
                 sectionSize);

    painter.setPen(textColor);
    painter.setFont(m_contentFont);
    painter.drawText(crect11, Qt::AlignLeft | Qt::AlignVCenter, recvTitle);
    painter.drawText(crect21, Qt::AlignLeft | Qt::AlignVCenter, sentTitle);
    painter.drawText(crect13, Qt::AlignLeft | Qt::AlignVCenter, recvTotalTitle);
    painter.drawText(crect23, Qt::AlignLeft | Qt::AlignVCenter, sentTotalTitle);

    painter.setPen(summaryColor);
    painter.setFont(m_subContentFont);
    painter.drawText(crect12, Qt::AlignLeft | Qt::AlignVCenter, recvContent);
    painter.drawText(crect22, Qt::AlignLeft | Qt::AlignVCenter, sentContent);
    painter.drawText(crect14, Qt::AlignLeft | Qt::AlignVCenter, recvTotalContent);
    painter.drawText(crect24, Qt::AlignLeft | Qt::AlignVCenter, sentTotalContent);

    QPainterPath path1, path2;
    path1.addEllipse(r1Ind);
    path2.addEllipse(r2Ind);

    painter.fillPath(path1, m_recvIndicatorColor);
    painter.fillPath(path2, m_sentIndicatorColor);

    // Draw background grid.
    painter.setRenderHint(QPainter::Antialiasing, false);
    QPen framePen;
    framePen.setColor(m_frameColor);
    framePen.setWidth(1);
    painter.setPen(framePen);

    int penSize = 1;
    int gridX = rect().x() + penSize + 3;
    int gridY = rect().y() + crect22.y() + crect22.height() + m_margin;
    int gridWidth =
        rect().width() - 3 - ((rect().width() - 3 - penSize) % (gridSize + penSize)) - penSize;
    int gridHeight = downloadRenderMaxHeight + uploadRenderMaxHeight + 4 * penSize;

    QPainterPath framePath;
    framePath.addRect(QRect(gridX, gridY, gridWidth, gridHeight));
    painter.drawPath(framePath);

    // Draw grid.
    QPen gridPen;
    QVector<qreal> dashes;
    qreal space = 2;
    dashes << 2 << space;
    gridPen.setColor(m_frameColor);
    gridPen.setWidth(penSize);
    gridPen.setDashPattern(dashes);
    painter.setPen(gridPen);

    int gridLineX = gridX;
    while (gridLineX + gridSize + penSize < gridX + gridWidth) {
        gridLineX += gridSize + penSize;
        painter.drawLine(gridLineX, gridY + 1, gridLineX, gridY + gridHeight - 1);
    }
    int gridLineY = gridY;
    while (gridLineY + gridSize + penSize < gridY + gridHeight) {
        gridLineY += gridSize + penSize;
        painter.drawLine(gridX + 1, gridLineY, gridX + gridWidth - 1, gridLineY);
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate((rect().width() - pointsNumber * 5) / 2 - 7, downloadWaveformsRenderOffsetY);
    painter.scale(1, -1);

    qreal devicePixelRatio = qApp->devicePixelRatio();
    qreal networkCurveWidth = 1.2;
    if (devicePixelRatio > 1) {
        networkCurveWidth = 2;
    }
    painter.setPen(QPen(downloadColor, networkCurveWidth));
    painter.setBrush(QBrush());
    painter.drawPath(downloadPath);

    painter.translate(0, uploadWaveformsRenderOffsetY);
    painter.scale(1, -1);

    painter.setPen(QPen(uploadColor, networkCurveWidth));
    painter.setBrush(QBrush());
    painter.drawPath(uploadPath);
}

void NetworkMonitor::changeFont(const QFont &font)
{
    m_sectionFont = font;
    m_sectionFont.setPointSize(m_sectionFont.pointSize() + 12);
    m_contentFont = font;
    m_contentFont.setWeight(QFont::Medium);
    m_contentFont.setPointSize(m_contentFont.pointSize() - 1);
    m_subContentFont = font;
    m_subContentFont.setPointSize(m_subContentFont.pointSize() - 1);
}
