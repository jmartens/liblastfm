/*
   Copyright 2009 Last.fm Ltd. 
      - Primarily authored by Max Howell, Jono Cole and Doug Mansell

   This file is part of liblastfm.

   liblastfm is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   liblastfm is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with liblastfm.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "Audioscrobbler.h"
#include "ScrobbleCache.h"
#include "../core/XmlQuery.h"
#include "../ws/ws.h"


namespace lastfm
{
    struct AudioscrobblerPrivate
    {
        AudioscrobblerPrivate(const QString& id)
                : id( id )
                , cache( ws::Username )
                , hard_failures( 0 )
        {}
        
        ~AudioscrobblerPrivate()
        {
        }

        const QString id;
        ScrobbleCache cache;
        uint hard_failures;
    };
}


lastfm::Audioscrobbler::Audioscrobbler( const QString& id )
        : d( new AudioscrobblerPrivate(id) )
{
}


lastfm::Audioscrobbler::~Audioscrobbler()
{
    delete d;
}

void lastfm::Audioscrobbler::onAnnounced()
{

    QNetworkReply* reply = static_cast<QNetworkReply*>(sender());
    // always enclose retrieval functions in a try block, as they will
    // throw if they can't parse the data
    try
    {
        lastfm::XmlQuery const lfm = lastfm::ws::parse( reply );
        qDebug() << lfm.text();
    }
    catch (std::runtime_error& e)
    {
        // if getSimilar() failed to parse the QNetworkReply, then e will
        // be of type lastfm::ws::ParseError, which derives
        // std::runtime_error
        qWarning() << e.what();
    }

}

void
lastfm::Audioscrobbler::nowPlaying( const Track& track )
{
    // deleting a reply cancels the request and disconnects all signals
    delete reply;
    reply = Track(track).nowPlaying();
    connect( reply, SIGNAL(finished()), SLOT(onAnnounced()) );
}


void
lastfm::Audioscrobbler::cache( const Track& track )
{
    d->cache.add( track );
}


void
lastfm::Audioscrobbler::cache( const QList<Track>& tracks )
{
    d->cache.add( tracks );
}


void lastfm::Audioscrobbler::onScrobbled()
{
    QNetworkReply* reply = static_cast<QNetworkReply*>(sender());
    // always enclose retrieval functions in a try block, as they will
    // throw if they can't parse the data
    try
    {
        lastfm::XmlQuery const lfm = lastfm::ws::parse( reply );
        qDebug() << lfm.text();

        // Remove submitted batch from cache
        d->cache.remove(m_batch);
        qDebug() << d->cache.tracks().count();
    }
    catch (std::runtime_error& e)
    {
        // if getSimilar() failed to parse the QNetworkReply, then e will
        // be of type lastfm::ws::ParseError, which derives
        // std::runtime_error
        qWarning() << e.what();
    }

}

void
lastfm::Audioscrobbler::submit()
{
    QList<Track> m_list = d->cache.tracks().mid(0,50);
    qDebug() << m_list.count();

    // make sure the batch is empty
    m_batch.clear();

    if (m_list.isEmpty())
        return;

    QMap<QString, QString> map;

    // Set the method (required)
    map["method"] = "track.scrobble";

    for (int i = 0; i < 50 && !m_list.isEmpty(); ++i)
    {
        QByteArray const N = QByteArray::number( i );

        Track t = m_list.takeFirst();

        // Required parameters
        map["artist[" + N + "]"] = t.artist();
        map["track[" + N + "]"] = t.title();
        map["timestamp[" + N + "]"] = QString::number(t.timestamp().toTime_t());

        // Optional parameters
        if (!t.album().isNull())
            map["album[" + N + "]"] = t.album();
/*
        if (t.albumArtist())
            map["albumArtist[" + N + "]"] = t.albumArtist();
*/
        if (t.duration() != 0)
            map["duration[" + N + "]"] = QString::number(t.duration());
/*
        if (t.streamId())
            map["streamId[" + N + "]"] = t.streamId();

        if (t.chosenByUser())
            map["chosenByUser[" + N + "]"] = t.chosenByUser();

        if (t.context())
            map["context[" + N + "]"] = t.context();
*/

        if (t.trackNumber() != 0)
            map["trackNumber[" + N + "]"] = QString::number(t.trackNumber());

        if (!t.mbid().isNull())
            map["mbid[" + N + "]"] = t.mbid();

        m_batch += t;

    }

    qDebug() << map;

    // deleting a reply cancels the request and disconnects all signals
    delete reply;
    reply = lastfm::ws::post(map);
    connect( reply, SIGNAL(finished()), SLOT(onScrobbled()) );
}


void
lastfm::Audioscrobbler::onError( Audioscrobbler::Error code )
{
    qDebug() << code; //TODO error text

    switch (code)
    {
        case Audioscrobbler::ErrorBannedClientVersion:
        case Audioscrobbler::ErrorInvalidSessionKey:
        case Audioscrobbler::ErrorBadTime:
            // np and submitter are in invalid state and won't send any requests
            // the app has to tell the user and let them decide what to do
            break;

        default:
            Q_ASSERT( false ); // you (yes you!) have missed an enum value out

        case Audioscrobbler::ErrorThreeHardFailures:
        case Audioscrobbler::ErrorBadSession:
//            handshake();
            break;
    }

    emit status( code );
}


#define SPLIT( x ) QList<QByteArray> const results = x.split( '\n' ); QByteArray const code =  results.value( 0 ); qDebug() << x.trimmed();

void
lastfm::Audioscrobbler::onNowPlayingReturn( const QByteArray& result )
{
}


void
lastfm::Audioscrobbler::onSubmissionReturn( const QByteArray& result )
{
}
