#include <stdexcept>

#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QThread>
#include <QWindow>

#include "../io/messages.hh"
#include "gui.hh"

#ifdef _WIN32
#   include <windows.h>
#endif

namespace clap {

   Gui::Gui(AbstractGuiListener &listener,
                        const QStringList &qmlImportPath)
      : AbstractGui(listener), _quickView(new QQuickView()) {

      _pluginProxy = std::make_unique<PluginProxy>(*this);
      _transportProxy = std::make_unique<TransportProxy>(*this);

      ////////////////////////
      // QML initialization //
      ////////////////////////

      auto qmlContext = _quickView->engine()->rootContext();

      for (const auto &str : qmlImportPath)
         _quickView->engine()->addImportPath(str);

      qmlContext->setContextProperty("plugin", _pluginProxy.get());
      qmlContext->setContextProperty("transport", _transportProxy.get());
   }

   Gui::~Gui() {}

   void Gui::setSkin(const std::string& skinUrl)
   {
      _quickView->setSource(QUrl(skinUrl.c_str()));
   }

   void Gui::defineParameter(const clap_param_info &paramInfo) {
      _pluginProxy->defineParameter(paramInfo);
   }

   void Gui::updateParameter(clap_id paramId, double value, double modAmount) {
      auto p = _pluginProxy->param(paramId);
      assert(p);
      if (!p)
         return;

      p->setValueFromPlugin(value);
      p->setModulationFromPlugin(modAmount);
   }

   void Gui::clearTransport() { _transportProxy->clear(); }

   void Gui::updateTransport(const clap_event_transport &transport) {
      _transportProxy->update(transport);
   }

   void Gui::showLater() {
      QMetaObject::invokeMethod(
         this,
         [this] {
            if (_hostWindow && _quickView)
               _quickView->show();
         },
         Qt::QueuedConnection);
   }

   bool Gui::attachCocoa(void *nsView) {
#ifdef Q_OS_MACOS
      _hostWindow.reset(QWindow::fromWinId(reinterpret_cast<WId>(nsView)));
      if (_hostWindow) {
         _quickView->setParent(_hostWindow.get());
         _quickView->show();
         return true;
      }
#endif
      return false;
   }

   bool Gui::attachWin32(clap_hwnd window) {
#ifdef Q_OS_WIN
      _hostWindow.reset(QWindow::fromWinId(reinterpret_cast<WId>(window)));
      if (_hostWindow) {
         _quickView->setParent(_hostWindow.get());
         showLater();
         return true;
      }
#endif
      return false;
   }

   bool Gui::attachX11(const char *displayName, unsigned long window) {
#ifdef Q_OS_LINUX
      // TODO: check the displayName
      _hostWindow.reset(QWindow::fromWinId(window));
      if (_hostWindow) {
         _quickView->setParent(_hostWindow.get());
         _quickView->show();
         qGuiApp->sync();
         return true;
      }
#endif
      return false;
   }

   bool Gui::wantsLogicalSize() noexcept {
#ifdef Q_OS_MACOS
      return true;
#else
      return false;
#endif
   }

   bool Gui::canResize()
   {
      return true;
   }

   bool Gui::size(uint32_t *width, uint32_t *height) {
      if (!_quickView)
         return false;

      while (_quickView->status() == QQuickView::Status::Loading) {
         QThread::sleep(10);
         qGuiApp->processEvents();
      }

      auto rootItem = _quickView->rootObject();
      if (!rootItem)
         return false;

      auto ratio = wantsLogicalSize() ? 1 : _quickView->devicePixelRatio();
      *width = rootItem->width() * ratio;
      *height = rootItem->height() * ratio;
      return true;
   }

   bool Gui::roundSize(uint32_t *width, uint32_t *height)
   {
      uint32_t w, h;
      if (!size(&w, &h))
         return false;

      if (*width < w)
         *width = w;
      if (*height < h)
         *height = h;

      double rw = *width / double(w);
      double rh = *height / double(h);
      double r = std::min<double>(rw, rh);
      *width = r * w;
      *height = r * h;
      return true;
   }

   bool Gui::setScale(double scale) { return false; }

   bool Gui::show() {
      _quickView->show();
      return true;
   }

   bool Gui::hide() {
      _quickView->hide();
      return true;
   }

   void Gui::destroy() {
      _quickView.reset();
      _hostWindow.reset();
      _transportProxy.reset();
      _pluginProxy.reset();
   }
} // namespace clap