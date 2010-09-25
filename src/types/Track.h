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
#ifndef LASTFM_TRACK_H
#define LASTFM_TRACK_H

#include <lastfm/Album>
#include <lastfm/Mbid>
#include <QDateTime>
#include <QDomElement>
#include <QExplicitlySharedDataPointer>
#include <QString>
#include <QMap>
#include <QUrl>


namespace lastfm {


struct TrackData : QSharedData
{
    TrackData();

    QString artist;
    QString album;
    QString title;
    uint trackNumber;
    uint duration;
    short source;
    short rating;
    QString mbid; /// musicbrainz id
    uint fpid;
    QUrl url;
    QDateTime time; /// the time the track was started at

    //FIXME I hate this, but is used for radio trackauth etc.
    QMap<QString,QString> extras;
    
    bool null;
};



/** Our track type. It's quite good, you may want to use it as your track type
  * in general. It is explicitly shared. Which means when you make a copy, they
  * both point to the same data still. This is like Qt's implicitly shared
  * classes, eg. QString, however if you mod a copy of a QString, the copy
  * detaches first, so then you have two copies. Our Track object doesn't
  * detach, which is very handy for our usage in the client, but perhaps not
  * what you want. If you need a deep copy for eg. work in a thread, call 
  * clone(). */
class LASTFM_DLLEXPORT Track
{
public:
    enum Source
    {
        // DO NOT UNDER ANY CIRCUMSTANCES CHANGE THE ORDER OR VALUES OF THIS ENUM!
        // you will cause broken settings and b0rked scrobbler cache submissions

        Unknown = 0,
        LastFmRadio,
        Player,
        MediaDevice,
        NonPersonalisedBroadcast, // eg Shoutcast, BBC Radio 1, etc.
        PersonalisedRecommendation, // eg Pandora, but not Last.fm
    };

    Track();
    explicit Track( const QDomElement& );
    
    /** if you plan to use this track in a separate thread, you need to clone it
      * first, otherwise nothing is thread-safe, not this creates a disconnected
      * Track object, modifications to this or that will not effect that or this
      */
    Track clone() const;

    /** this track and that track point to the same object, so they are the same
      * in fact. This doesn't do a deep data comparison. So even if all the 
      * fields are the same it will return false if they aren't in fact spawned
      * from the same initial Track object */
    bool operator==( const Track& that ) const
    {
        return this->d == that.d;
    }
    bool operator!=( const Track& that ) const
    {
        return !operator==( that );
    }

    /** only a Track() is null */
    bool isNull() const { return d->null; }

    Artist artist() const { return Artist( d->artist ); }
    Album album() const { return Album( artist(), d->album ); }
    QString title() const
    {
        /** if no title is set, return the musicbrainz unknown identifier
          * in case some part of the GUI tries to display it anyway. Note isNull
          * returns false still. So you should have queried this! */
        return d->title.isEmpty() ? "[unknown]" : d->title;
    }
    uint trackNumber() const { return d->trackNumber; }
    uint duration() const { return d->duration; } /// in seconds
    Mbid mbid() const { return Mbid(d->mbid); }
    QUrl url() const { return d->url; }
    QDateTime timestamp() const { return d->time; }
    Source source() const { return (Source)d->source; }
    uint fingerprintId() const { return d->fpid; }

    QString durationString() const { return durationString( d->duration ); }
    static QString durationString( int seconds );

    /** default separator is an en-dash */
    QString toString( const QChar& separator = QChar(8211) ) const;
    /** the standard representation of this object as an XML node */
    QDomElement toDomElement( class QDomDocument& ) const;
    
    QString extra( const QString& key ) const{ return d->extras[ key ]; }

    bool operator<( const Track &that ) const
    {
        return this->d->time < that.d->time;
    }
    
    bool isMp3() const;
    
    operator QVariant() const { return QVariant::fromValue( *this ); }
    
//////////// lastfm::Ws
    
    /** See last.fm/api Track section */
    QNetworkReply* share( const class User& recipient, const QString& message = "" );

    /** you can get any QNetworkReply TagList using Tag::list( QNetworkReply* ) */
    QNetworkReply* getTags() const; // for the logged in user
    QNetworkReply* getTopTags() const;

    /** you can only add 10 tags, we submit everything you give us, but the
      * docs state 10 only. Will return 0 if the list is empty. */
    QNetworkReply* addTags( const QStringList& ) const;
    /** will return 0 if the string is "" */
    QNetworkReply* removeTag( const QString& ) const;

    /** Scrobble a single track. Timestamp is the UTC unix time the track
      * _started_
      *
      * The track must have been stamped via MutableTrack::stamp() or you'll
      * get NULL back.
      *
      * NOTE Generally you don't want to use this. It's easier to use an 
      * instance of the Audioscrobbler class as it does the hard stuff for you
      * eg. caching, retries, etc. It's solid stuff. */
    QNetworkReply* scrobble() const;

    /** Scrobble lots of tracks. The purpose of this method is to scrobble
      * tracks that could not be submitted because Last.fm was not accessible.
      *
      * So probably your flow is this: Try Track.scrobble, if that fails cache
      * the track (see: ScrobbleCache). Then try to scrobble the cache every
      * so often.
      *
      * NOTE Generally you don't want to use this. It's easier to use an 
      * instance of the Audioscrobbler class which does all the hard bits for
      * you. */
    static QNetworkReply* scrobbleBatch( const QList<Track>& tracks );

    /** the url for this track's page at last.fm */
    QUrl www() const;

protected:
    QExplicitlySharedDataPointer<TrackData> d;
    QMap<QString, QString> params( const QString& method, bool use_mbid = false ) const;
    
private:
    Track( TrackData* that_d ) : d( that_d )
    {}
};



/** This class allows you to change Track objects, it is easy to use:
  * MutableTrack( some_track_object ).setTitle( "Arse" );
  *
  * We have a separate MutableTrack class because in our usage, tracks
  * only get mutated once, and then after that, very rarely. This pattern
  * encourages such usage, which is generally sensible. You can feel more
  * comfortable that the data hasn't accidently changed behind your back.
  */
class LASTFM_DLLEXPORT MutableTrack : public Track
{
public:
    MutableTrack()
    {
        d->null = false;
    }

    /** NOTE that passing a Track() to this ctor will automatically make it non
      * null. Which may not be what you want. So be careful
      * Rationale: this is the most maintainable way to do it 
      */
    MutableTrack( const Track& that ) : Track( that )
    {
        d->null = false;
    }
    
    void setArtist( QString artist ) { d->artist = artist.trimmed(); }
    void setAlbum( QString album ) { d->album = album.trimmed(); }
    void setTitle( QString title ) { d->title = title.trimmed(); }
    void setTrackNumber( uint n ) { d->trackNumber = n; }
    void setDuration( uint duration ) { d->duration = duration; }
    void setUrl( QUrl url ) { d->url = url; }
    void setSource( Source s ) { d->source = s; }
    
    void setMbid( Mbid id ) { d->mbid = id; }
    void setFingerprintId( uint id ) { d->fpid = id; }
    
    /** you also must scrobble this track for the love to become permenant */
    QNetworkReply* love();
    QNetworkReply* ban();

    /** currently doesn't work, as there is no webservice */
    void unlove();
    
    void stamp() { d->time = QDateTime::currentDateTime(); }

    void setExtra( const QString& key, const QString& value ) { d->extras[key] = value; }
    void removeExtra( QString key ) { d->extras.remove( key ); }
};


inline 
TrackData::TrackData() 
             : trackNumber( 0 ),
               duration( 0 ),
               source( Track::Unknown ),
               rating( 0 ),
               fpid( -1 ),
               null( false )
{}


} //namespace lastfm


inline QDebug operator<<( QDebug d, const lastfm::Track& t )
{
    return !t.isNull() 
            ? d << t.toString( '-' ) << t.url()
            : d << "Null Track object";
}


Q_DECLARE_METATYPE( lastfm::Track );

#endif //LASTFM_TRACK_H
