/*
 * TwitCoop: Desktop interface to Twitter Mobile Web.
 *
 * Copyright (c) 2011 Remko Tronçon
 * Licensed under the GNU General Public License v3.
 */

// TODO:
// - Use notification instead of timer to click the timeline-refresh
// 	=> Wait for the new mobile interface. Might not be necessary anymore.
// - Hide 'show twitter in standard/mobile'
// 	=> Wait for the new mobile interface. Might not be necessary anymore.

#include <QtGui>
#include <QtWebKit>

#include "flickcharm.h"

static const QString APPLICATION_NAME = QString("TwitCoop");
static const QString ORGANIZATION_NAME = QString("El Tramo");

static const int REFRESH_TIME_SECONDS = 60;
static const int CHECK_NEEDS_UPDATE_SECONDS = 10;
static const int RELOAD_ON_ERROR_SECONDS = 10;

class CookieJar : public QNetworkCookieJar {
	public:
		CookieJar(QSettings* settings) : settings(settings) {
			QVariant cookiesSetting = settings->value("cookies", QVariant(QList<QVariant>()));
			QList<QVariant> cookies = cookiesSetting.toList();
			QList<QNetworkCookie> parsedCookies;
			for (QList<QVariant>::const_iterator i = cookies.begin(); i != cookies.end(); ++i) {
				parsedCookies.append(QNetworkCookie::parseCookies(i->toByteArray()));
			}
			setAllCookies(parsedCookies);
		}

		~CookieJar() {
			QList<QNetworkCookie> cookies = allCookies();
			QList<QVariant> serializedCookies;
			for (QList<QNetworkCookie>::const_iterator i = cookies.begin(); i != cookies.end(); ++i) {
				serializedCookies.append(i->toRawForm());
			}
			settings->setValue("cookies", serializedCookies);
		}

	private:
		QSettings* settings;
};

class WebPage : public QWebPage {
	public:
		WebPage(QObject* parent) : QWebPage(parent) {
		}

	private:
		virtual void javaScriptAlert(QWebFrame*, const QString & msg) {
			qDebug() << "JS Alert:" << msg;
		}

		virtual void javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID) {
			qDebug() << "JS Message:" << sourceID << lineNumber << message;
		}
};

class TwitCoopWindow : public QMainWindow {
		Q_OBJECT
	public: 
		TwitCoopWindow() {
			setWindowTitle(APPLICATION_NAME);
#ifndef Q_WS_X11
			setWindowFlags(Qt::Tool);
#endif

			settings = new QSettings(ORGANIZATION_NAME, APPLICATION_NAME);
			cookieJar = new CookieJar(settings);

			widgetStack = new QStackedWidget(this);
			setCentralWidget(widgetStack);
			
			QWidget* splash = new QWidget(widgetStack);
			QVBoxLayout* layout = new QVBoxLayout(splash);
			layout->addStretch();
			QLabel* splashLabel = new QLabel(splash);
			splashLabel->setText("<html><center><img src=':/twitter-blue.png' width='128' height='128'/><h1>TwitCoop</h1><p>&copy; 2011 Remko Tron&ccedil;on<br/><a href='http://el-tramo.be'>http://el-tramo.be</a></p></center></html>");
			layout->addWidget(splashLabel);
			layout->addStretch();
			throbberLabel = new QLabel(splash);
			throbberLabel->setAlignment(Qt::AlignHCenter);
			throbberLabel->setMovie(new QMovie(":/throbber.gif", QByteArray(), throbberLabel));
			throbberLabel->movie()->start();
			layout->addWidget(throbberLabel);
			layout->addStretch();

			widgetStack->addWidget(splash);

			web = new QWebView(widgetStack);
			web->setPage(new WebPage(web));
			web->setZoomFactor(settings->value("zoom", 0.7).toFloat());
			web->installEventFilter(this);
			web->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
			web->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
			web->page()->networkAccessManager()->setCookieJar(cookieJar);
			connect(web, SIGNAL(linkClicked(const QUrl&)), SLOT(handleLinkClicked(const QUrl&)));
			connect(web, SIGNAL(loadStarted()), SLOT(handleLoadStarted()));
			connect(web, SIGNAL(loadFinished(bool)), SLOT(handleLoadFinished(bool)));
			connect(web, SIGNAL(loadProgress(int)), SLOT(handleLoadProgress(int)));
			widgetStack->addWidget(web);
			web->setUrl(QUrl("http://mobile.twitter.com/session"));
			flickCharm.activateOn(web);

			tray = new QSystemTrayIcon(QIcon(":/twitter-blue.png"), this);
			mainMenu = new QMenu(APPLICATION_NAME, this);
			showTweetBoxAction = new QAction(tr("Show Tweet box"), mainMenu);
			showTweetBoxAction->setCheckable(true);
			showTweetBoxAction->setChecked(settings->value("show-tweetbox", true).toBool());
			connect(showTweetBoxAction, SIGNAL(toggled(bool)), SLOT(updateTweetBoxVisibility()));
			mainMenu->addAction(showTweetBoxAction);
			mainMenu->addSeparator();
			mainMenu->addAction(tr("Larger"), this, SLOT(handleLarger()));
			mainMenu->addAction(tr("Smaller"), this, SLOT(handleSmaller()));
			mainMenu->addSeparator();
			mainMenu->addAction(tr("E&xit"), QApplication::instance(), SLOT(quit()));
			tray->setContextMenu(mainMenu);
			connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), SLOT(handleTrayActivated(QSystemTrayIcon::ActivationReason)));
			tray->show();

			QTimer* reloadTimer = new QTimer(this);
			reloadTimer->setInterval(CHECK_NEEDS_UPDATE_SECONDS*1000);
			connect(reloadTimer, SIGNAL(timeout()), SLOT(handleReloadTimerTick()));
			reloadTimer->start();

			connect(QApplication::instance(), SIGNAL(focusChanged(QWidget*, QWidget*)), SLOT(handleFocusChanged(QWidget*, QWidget*)));
			connect(QApplication::instance(), SIGNAL(aboutToQuit()), SLOT(handleAboutToQuit()));

			if (!restoreGeometry(settings->value("geometry").toByteArray())) {
				resize(QSize(200, QApplication::desktop()->screenGeometry().height() - 60));
			}
		}

		~TwitCoopWindow() {
			delete cookieJar;
			delete settings;
		}

	private slots:
		void handleLinkClicked(const QUrl& url) {
			if (url.host() == "mobile.twitter.com") {
				if (url.path().startsWith("/settings/change_ui")) {
					return;
				}
				// Not using setUrl, because it blanks the webpage first
				web->page()->mainFrame()->evaluateJavaScript(QString("window.location=\"%1\";").arg(url.toString()));
			}
			else {
				QDesktopServices::openUrl(url);
			}
		}

		void reload() {
			web->setUrl(web->url());
		}

		void handleLoadStarted() {
			QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));

			statusBar()->clearMessage();
			statusBar()->hide();
		}

		void handleLoadProgress(int) {
			updateTweetBoxVisibility();
		}

		void handleLoadFinished(bool ok) {
			if (!ok) {
				statusBar()->showMessage("Connection error. Retrying ...");
				statusBar()->show();
				QTimer::singleShot(RELOAD_ON_ERROR_SECONDS*1000, this, SLOT(reload()));
				return;
			}

			updateTweetBoxVisibility();
			QWebElement head = web->page()->mainFrame()->documentElement().findFirst("head");
			if (!head.isNull()) {
				head.evaluateJavaScript(
					"var s = document.createElement('style');"
					"s.type = 'text/css';"
					"s.appendChild(document.createTextNode('.timeline-refresh { display: none; }'));"
					"this.appendChild(s);");
			}
			updateRefreshTime();
			QApplication::restoreOverrideCursor();

			if (widgetStack->currentWidget() != web) {
				widgetStack->setCurrentWidget(web);
				throbberLabel->movie()->stop();
			}
		}

		void updateRefreshTime() {
			web->page()->mainFrame()->evaluateJavaScript(QString("if (typeof(Tweets) != 'undefined') { Tweets.REFRESH_TIME = %1; }").arg(REFRESH_TIME_SECONDS*1000));
		}

		void updateTweetBoxVisibility() {
			QWebElement tweetBox = web->page()->mainFrame()->documentElement().findFirst("div[class=tweetbox]");
			if (!tweetBox.isNull()) {
				QString displayPropertyValue = isTweetBoxVisible() ? "block" : "none";
				if (displayPropertyValue != tweetBox.styleProperty("display", QWebElement::InlineStyle)) {
					tweetBox.setStyleProperty("display", displayPropertyValue);
				}
			}
		}

		void handleReloadTimerTick() {
			updateRefreshTime(); // Setting this after document load is to early, so resetting it
			QWebElement button = web->page()->mainFrame()->documentElement().findFirst("a[class=update]");
			if (!button.isNull()) {
				button.evaluateJavaScript("var evObj = document.createEvent('MouseEvents');evObj.initEvent( 'click', true, true );this.dispatchEvent(evObj);");
			}
		}

		void handleFocusChanged(QWidget* from, QWidget* to) {
			if (!from && isAncestorOf(to)) {
				// Focus in
			}
			if (!to && isAncestorOf(from)) {
				// Focus out
			}
		}

		void handleTrayActivated(QSystemTrayIcon::ActivationReason reason) {
			if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::MiddleClick) {
				show();
			}
		}

		void handleLarger() {
			web->setZoomFactor(web->zoomFactor() + 0.1);
		}

		void handleSmaller() {
			if (web->zoomFactor() > 0.1) {
				web->setZoomFactor(web->zoomFactor() - 0.1);
			}
		}

		void handleAboutToQuit() {
			settings->setValue("geometry", saveGeometry());
			settings->setValue("show-tweetbox", showTweetBoxAction->isChecked());
			settings->setValue("zoom", web->zoomFactor());
		}

		bool isTweetBoxVisible() const {
			return showTweetBoxAction->isChecked() || web->url().path().endsWith("compose_reply");
		}
	
	private:
		bool eventFilter(QObject*, QEvent* event) {
			if (event->type() == QEvent::ContextMenu) {
				mainMenu->exec(static_cast<QContextMenuEvent*>(event)->globalPos());
				return true;
			}
			return false;
		}

	private:
		FlickCharm flickCharm;
		QSettings* settings;
		QSystemTrayIcon* tray;
		QStackedWidget* widgetStack;
		QLabel* throbberLabel;
		QWebView* web;
		QMenu* mainMenu;
		CookieJar* cookieJar;
		QAction* showTweetBoxAction;
};

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	QApplication::setApplicationName(APPLICATION_NAME);
	TwitCoopWindow w;
	w.show();
	return app.exec();
}

#include "TwitCoop.moc"
