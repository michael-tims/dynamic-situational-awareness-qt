// Copyright 2017 ESRI
//
// All rights reserved under the copyright laws of the United States
// and applicable international laws, treaties, and conventions.
//
// You may freely redistribute and use this sample code, with or
// without modification, provided you include the original copyright
// notice and use restrictions.
//
// See the Sample code usage restrictions document for further information.
//

#include "MessageFeedsController.h"
#include "Message.h"
#include "MessagesOverlay.h"
#include "MessageListener.h"
#include "MessageFeedListModel.h"
#include "MessageFeed.h"
#include "MessageSender.h"
#include "LocationBroadcast.h"

#include "ToolResourceProvider.h"
#include "ToolManager.h"

#include "DictionarySymbolStyle.h"
#include "DictionaryRenderer.h"
#include "SimpleRenderer.h"
#include "PictureMarkerSymbol.h"

#include <QUdpSocket>

using namespace Esri::ArcGISRuntime;

const QString MessageFeedsController::RESOURCE_DIRECTORY_PROPERTYNAME = "ResourceDirectory";
const QString MessageFeedsController::MESSAGE_FEED_UDP_PORTS_PROPERTYNAME = "MessageFeedUdpPorts";
const QString MessageFeedsController::MESSAGE_FEEDS_PROPERTYNAME = "MessageFeeds";
const QString MessageFeedsController::LOCATION_BROADCAST_CONFIG_PROPERTYNAME = "LocationBroadcastConfig";

/*!
   \brief Constructs a default MessageFeedsController with an optional \a parent.
 */
MessageFeedsController::MessageFeedsController(QObject* parent) :
  Toolkit::AbstractTool(parent),
  m_messageFeeds(new MessageFeedListModel(this)),
  m_locationBroadcast(new LocationBroadcast(this))
{
  Toolkit::ToolManager::instance().addTool(this);

  connect(Toolkit::ToolResourceProvider::instance(), &Toolkit::ToolResourceProvider::geoViewChanged, this, [this]
  {
    setGeoView(Toolkit::ToolResourceProvider::instance()->geoView());
  });
}

/*!
   \brief Destructor
 */
MessageFeedsController::~MessageFeedsController()
{
}

/*!
   \brief Sets the GeoView for the MessageOverlay objects to \a geoView.
 */
void MessageFeedsController::setGeoView(GeoView* geoView)
{
  m_geoView = geoView;
}

/*!
   \brief Returns the message feeds list model.
 */
QAbstractListModel* MessageFeedsController::messageFeeds() const
{
  return m_messageFeeds;
}

/*!
   \brief Returns the list of message listener objects that exist for
   the message feeds.
 */
QList<MessageListener*> MessageFeedsController::messageListeners() const
{
  return m_messageListeners;
}

/*!
   \brief Adds and registers a message listener object to be used by the message feeds.

   \list
     \li \a messageListener - The message listener object to add to the controller.
   \endlist
 */
void MessageFeedsController::addMessageListener(MessageListener* messageListener)
{
  if (!messageListener)
    return;

  m_messageListeners.append(messageListener);

  connect(messageListener, &MessageListener::messageReceived, this, [this](const QByteArray& message)
  {
    Message m = Message::create(message);
    if (m.isEmpty())
      return;

    if (m_locationBroadcast->isEnabled())
    {
      if (m_locationBroadcast->message().messageId() == m.messageId()) // do not display our own location broadcast message
        return;
    }

    MessageFeed* messageFeed = m_messageFeeds->messageFeedByType(m.messageType());
    if (!messageFeed)
      return;

    messageFeed->messagesOverlay()->addMessage(m);
  });
}

/*!
   \brief Removes a message listener object from the controller.

   \list
     \li \a messageListener - The message listener object to remove from the controller.
   \endlist
 */
void MessageFeedsController::removeMessageListener(MessageListener* messageListener)
{
  if (!messageListener)
    return;

  m_messageListeners.removeOne(messageListener);

  disconnect(messageListener, &MessageListener::messageReceived, this, nullptr);
}

/*!
   \brief Returns the name of the message feeds controller.
 */
QString MessageFeedsController::toolName() const
{
  return QStringLiteral("messages");
}

/*!
   \brief Sets \a properties for configuring the message feeds controller.

   Applicable properties are:
   \list
     \li RESOURCE_DIRECTORY_PROPERTYNAME - The resource directory where symbol style files are located.
     \li MESSAGE_FEED_UDP_PORTS_PROPERTYNAME - The UDP ports for listening to message feeds.
     \li MESSAGE_FEEDS_PROPERTYNAME - A list of message feed configurations.
     \li LOCATION_BROADCAST_CONFIG_PROPERTYNAME - The location broadcast configuration details.
   \endlist
 */
void MessageFeedsController::setProperties(const QVariantMap& properties)
{
  setResourcePath(properties[RESOURCE_DIRECTORY_PROPERTYNAME].toString());

  // parse and add message listeners on specified UDP ports
  const auto messageFeedUdpPorts = properties[MESSAGE_FEED_UDP_PORTS_PROPERTYNAME].toStringList();
  for (const auto& udpPort : messageFeedUdpPorts)
  {
    QUdpSocket* udpSocket = new QUdpSocket(this);
    udpSocket->bind(udpPort.toInt(), QUdpSocket::DontShareAddress | QUdpSocket::ReuseAddressHint);

    addMessageListener(new MessageListener(udpSocket, this));
  }

  // parse and add message feeds
  const auto messageFeeds = properties[MESSAGE_FEEDS_PROPERTYNAME].toStringList();
  for (const auto& messageFeed : messageFeeds)
  {
    const auto& messageFeedConfig = messageFeed.split(":");
    if (messageFeedConfig.size() != 3)
      continue;

    const auto feedName = messageFeedConfig[0];
    const auto feedType = messageFeedConfig[1];
    const auto rendererInfo = messageFeedConfig[2];

    MessagesOverlay* overlay = new MessagesOverlay(m_geoView, createRenderer(rendererInfo, this));
    MessageFeed* feed = new MessageFeed(feedName, feedType, overlay, this);
    m_messageFeeds->append(feed);
  }

  const auto locationBroadcastConfig = properties[LOCATION_BROADCAST_CONFIG_PROPERTYNAME].toStringList();
  if (locationBroadcastConfig.size() == 2)
  {
    m_locationBroadcast->setMessageType(locationBroadcastConfig.at(0));
    m_locationBroadcast->setUdpPort(locationBroadcastConfig.at(1).toInt());
  }
}

/*!
   \brief Sets the data path to be used for symbol style resources as \a resourcePath.
 */
void MessageFeedsController::setResourcePath(const QString& resourcePath)
{
  if (resourcePath == m_resourcePath)
    return;

  m_resourcePath = resourcePath;

  emit propertyChanged(RESOURCE_DIRECTORY_PROPERTYNAME, resourcePath);
}

LocationBroadcast* MessageFeedsController::locationBroadcast() const
{
  return m_locationBroadcast;
}

/*!
   \brief Returns \c true if the location broadcast is enabled.

   \sa LocationBroadcast::isEnabled
 */
bool MessageFeedsController::isLocationBroadcastEnabled() const
{
  return m_locationBroadcast->isEnabled();
}

/*!
   \brief Sets whether the location broadcast is \a enabled.

   \sa LocationBroadcast::setEnabled
 */
void MessageFeedsController::setLocationBroadcastEnabled(bool enabled)
{
  if (m_locationBroadcast->isEnabled() == enabled)
    return;

  m_locationBroadcast->setEnabled(enabled);

  emit locationBroadcastEnabledChanged();
}

/*!
   \brief Returns the location broadcast frequency.

   \sa LocationBroadcast::frequency
 */
int MessageFeedsController::locationBroadcastFrequency() const
{
  return m_locationBroadcast->frequency();
}

/*!
   \brief Sets the location broadcast message frequency to \a frequency.

   \sa LocationBroadcast::setFrequency
 */
void MessageFeedsController::setLocationBroadcastFrequency(int frequency)
{
  if (m_locationBroadcast->frequency() == frequency)
    return;

  m_locationBroadcast->setFrequency(frequency);

  emit locationBroadcastFrequencyChanged();
}

/*!
   \internal
   \brief Creates and returns a renderer from the provided \a rendererInfo with an optional \a parent.

   The \a rendererInfo parameter can be the symbol specification type (i.e. "mil2525c_b2" or "mil2525d") or
   it can be the name of an image file located in the ":/Resources/icons/xhdpi/message" path, such
   as ":/Resources/icons/xhdpi/message/enemycontact1600.png".
 */
Renderer* MessageFeedsController::createRenderer(const QString& rendererInfo, QObject* parent) const
{
  // hold mil2525 symbol styles as statics to be shared by multiple renderers if needed
  static DictionarySymbolStyle* dictionarySymbolStyleMil2525c = nullptr;
  static DictionarySymbolStyle* dictionarySymbolStyleMil2525d = nullptr;

  if (rendererInfo.compare("mil2525c", Qt::CaseInsensitive) == 0)
  {
    if (!dictionarySymbolStyleMil2525c)
      dictionarySymbolStyleMil2525c = new DictionarySymbolStyle("mil2525c_b2", m_resourcePath + "/styles/mil2525c_b2.stylx", parent);

    return new DictionaryRenderer(dictionarySymbolStyleMil2525c, parent);
  }
  else if (rendererInfo.compare("mil2525d", Qt::CaseInsensitive) == 0)
  {
    if (!dictionarySymbolStyleMil2525d)
      dictionarySymbolStyleMil2525d = new DictionarySymbolStyle("mil2525d", m_resourcePath + "/styles/mil2525d.stylx", parent);

    return new DictionaryRenderer(dictionarySymbolStyleMil2525d, parent);
  }

  // else default to simple renderer with picture marker symbol
  PictureMarkerSymbol* symbol = new PictureMarkerSymbol(QImage(QString(":/Resources/icons/xhdpi/message/%1").arg(rendererInfo)), parent);
  symbol->setWidth(40.0f);
  symbol->setHeight(40.0f);
  return new SimpleRenderer(symbol, parent);
}