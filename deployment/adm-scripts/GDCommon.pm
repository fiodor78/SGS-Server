#!/usr/bin/perl

package GDCommon;

use LWP::UserAgent;
use HTTP::Request;
use HTTP::Request::Common;

$hostname=`/bin/hostname| cut -d"." -f1`;
chomp($hostname);

$hostname_internal=`/sbin/ifconfig|/bin/grep '192.168.0.'|/bin/awk '{print \$2}'|cut -d":" -f2`;
chomp($hostname_internal);


sub alert_mail 
{
	my($subject,$content)=@_;
	open(OUT,"|/bin/mail -s \"$subject\" servers\@ganymede.com.pl");
	print OUT $content;
	close(OUT);
}

sub alert_mail_www 
{
	my($subject,$content)=@_;
	open(OUT,"|/bin/mail -s \"$subject\" wwwdevel\@ganymede.com.pl");
	print OUT $content;
	close(OUT);
}

sub alert_sms 
{
	my $content=$_[0];
	my $to=$_[1];
	my $ua=LWP::UserAgent->new(timeout=>30);
	if(!$to)
	{
		$to='critical_servers';
	}
	$ua->request(POST 'http://174.123.95.114/add_alert.php',[from=>$hostname,to=>$to,type=>'010',topic=>"",message=>$content]);
}

sub gen_scriptname 
{
	my $scriptname=$0;
	$scriptname =~ m/\/([^\/]+)$/;
	return $1;
}

return 1;
