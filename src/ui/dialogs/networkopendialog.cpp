#include "ui/dialogs/networkopendialog.h"
#include "playback/media/mediasource.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

NetworkOpenDialog::NetworkOpenDialog(QWidget *parent) : QDialog(parent) {
    setObjectName(QStringLiteral("networkOpenDialog"));
    setWindowTitle(tr("打开网络地址"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(620, 430);
    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(tr("媒体地址："), this);
    address_ = new QLineEdit(this);
    address_->setObjectName(QStringLiteral("networkAddress"));
    address_->setAccessibleName(tr("媒体网络地址"));
    address_->setPlaceholderText(QStringLiteral("https://… / rtsp://… / rtmp://…"));
    address_->setClearButtonEnabled(true);
    label->setBuddy(address_);
    auto *hint = new QLabel(tr("支持 HTTP(S)、RTSP(S)、RTMP(S) 媒体地址与直播流。"), this);
    hint->setWordWrap(true);

    auto *advanced = new QGroupBox(tr("网络选项（本次运行有效）"), this);
    auto *advancedForm = new QFormLayout(advanced);
    proxy_ = new QLineEdit(advanced);
    proxy_->setObjectName(QStringLiteral("networkProxy"));
    proxy_->setAccessibleName(tr("HTTP 代理"));
    proxy_->setPlaceholderText(QStringLiteral("http://127.0.0.1:7890"));
    advancedForm->addRow(tr("代理："), proxy_);
    headers_ = new QPlainTextEdit(advanced);
    headers_->setObjectName(QStringLiteral("networkHeaders"));
    headers_->setAccessibleName(tr("HTTP 请求头"));
    headers_->setPlaceholderText(tr("每行一个 Header: value，例如 Authorization: Bearer …"));
    headers_->setMaximumHeight(110);
    advancedForm->addRow(tr("请求头："), headers_);
    auto *cookiesRow = new QWidget(advanced);
    auto *cookiesLayout = new QHBoxLayout(cookiesRow);
    cookiesLayout->setContentsMargins(0, 0, 0, 0);
    cookies_ = new QLineEdit(cookiesRow);
    cookies_->setObjectName(QStringLiteral("networkCookies"));
    cookies_->setAccessibleName(tr("Cookie 文件"));
    auto *browse = new QPushButton(tr("浏览…"), cookiesRow);
    cookiesLayout->addWidget(cookies_, 1);
    cookiesLayout->addWidget(browse);
    advancedForm->addRow(tr("Cookie 文件："), cookiesRow);
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, tr("选择 Cookie 文件"));
        if (!path.isEmpty())
            cookies_->setText(path);
    });
    auto *security = new QLabel(
        tr("请求头可能包含凭据；只填入可信值。选项不会写入会话文件，重启后需重新输入。"), advanced);
    security->setWordWrap(true);
    advancedForm->addRow(security);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
    auto *open = buttons->button(QDialogButtonBox::Open);
    open->setObjectName(QStringLiteral("openNetworkButton"));
    open->setText(tr("播放"));
    open->setAccessibleName(tr("播放网络媒体"));
    open->setToolTip(tr("输入有效的 HTTP(S)、RTSP(S) 或 RTMP(S) 地址后播放"));
    open->setEnabled(false);
    connect(address_, &QLineEdit::textChanged, this,
            [this, open] { open->setEnabled(!mediaUrl().isEmpty()); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (!mediaUrl().isEmpty())
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(label);
    layout->addWidget(address_);
    layout->addWidget(hint);
    layout->addWidget(advanced);
    layout->addWidget(buttons);
    address_->setFocus();
}

QString NetworkOpenDialog::mediaUrl() const { return MediaSource::networkUrl(address_->text()); }

NetworkOptions NetworkOpenDialog::networkOptions() const {
    NetworkOptions options;
    options.proxy = proxy_->text().trimmed();
    options.cookiesFile = cookies_->text().trimmed();
    const auto lines = headers_->toPlainText().split(QLatin1Char('\n'));
    for (const auto &line : lines) {
        const auto trimmed = line.trimmed();
        if (!trimmed.isEmpty() && options.headers.size() < 32)
            options.headers.append(trimmed);
    }
    return options;
}

void NetworkOpenDialog::setNetworkOptions(const NetworkOptions &options) {
    proxy_->setText(options.proxy);
    headers_->setPlainText(options.headers.join(QLatin1Char('\n')));
    cookies_->setText(options.cookiesFile);
}
