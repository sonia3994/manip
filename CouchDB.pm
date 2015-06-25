#------------------------------------------------------------------------------
# File:         CouchDB.pm
#
# Description:  CouchDB interface
#
# Revisions:    2013/08/07 - PH Created
#
# Notes:        based on code from http://wiki.apache.org/couchdb/Getting_started_with_Perl
#------------------------------------------------------------------------------
package CouchDB;

use strict;
use warnings;

use LWP::UserAgent;

#------------------------------------------------------------------------------
# Create new CouchDB object
# Inputs: 0) CouchDB class name, 1) IP, 2) port, 3) realm, 4) userID, 5) password
sub new($$$;$$$)
{
    my ($class, $host, $port, $realm, $user, $pass) = @_;
    
    my $ua = LWP::UserAgent->new;
    $ua->timeout(10);
    $ua->env_proxy;
    $ua->credentials("$host:$port", $realm, $user, $pass) if $realm;

    return bless {
        ua       => $ua,
        host     => $host,
        port     => $port,
        base_uri => "http://$host:$port/",
    }, $class;
}

#------------------------------------------------------------------------------
# Get user agent
# Inputs: 0) CouchDB ref
sub ua($) { shift->{ua} }

#------------------------------------------------------------------------------
# Get base URI
# Inputs: 0) CouchDB ref
sub base($) { shift->{base_uri} }

#------------------------------------------------------------------------------
# HTTP request
# Inputs: 0) CouchDB ref, 1) HTTP method, 2) URI, 3) JSON content
# Returns: 0) response content, 1) error status
sub request($$$;$)
{
    my ($self, $method, $uri, $content) = @_;
    
    my $full_uri = $self->base . $uri;
    my $req;
    
    if (defined $content) {
        $req = HTTP::Request->new( $method, $full_uri, undef, $content );
        $req->header('Content-Type' => 'application/json');
    } else {
        $req = HTTP::Request->new( $method, $full_uri );
    }
    
    my $response = $self->ua->request($req);

    if ($response->is_success or not wantarray) {
        return $response->decoded_content;
    } else {
        return($response->decoded_content, $response->status_line);
    }
}

#------------------------------------------------------------------------------
# Delete entry from database
# Inputs: 0) CouchDB ref, 1) URL
# Returns: 0) response content, 1) error status
sub Delete($$)
{
    my ($self, $url) = @_;
    $self->request(DELETE => $url);
}

#------------------------------------------------------------------------------
# Get database entry
# Inputs: 0) CouchDB ref, 1) URL
# Returns: 0) response content, 1) error status
sub Get($$)
{
    my ($self, $url) = @_;
    return $self->request(GET => $url);
}

#------------------------------------------------------------------------------
# Put database entry (create or update)
# Inputs: 0) CouchDB ref, 1) URL
# Returns: 0) response content, 1) error status
sub Put($$)
{
    my ($self, $url, $json) = @_;
    return $self->request(PUT => $url, $json);
}

#------------------------------------------------------------------------------
# Post database entry (create)
# Inputs: 0) CouchDB ref, 1) URL
# Returns: 0) response content, 1) error status
sub Post($$)
{
    my ($self, $url, $json) = @_;
    return $self->request(POST => $url, $json);
}

#------------------------------------------------------------------------------
1; #end
