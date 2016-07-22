#!/usr/bin/perl

use lib "/home/servers/adm-scripts";
use GDCommon;

$leave_count=6;

chdir("/opt/servers/bin");

@nodes=<*>;
foreach $node (@nodes) {
 if((-d $node) and ($node ne ".") and ($node ne "..")) {
  chdir($node);
  @cores=`/bin/ls core.* -r -t 2>/dev/null`;
  if(@cores>$leave_count)
  {
   @new_cores=splice(@cores,-$leave_count);
   foreach $core (@cores)
   {
    chomp($core);
    $cmd="/bin/rm \"$core\"";
    `$cmd`;
   }
   foreach $core (@new_cores)
   {
    chomp($core);
    $cmd="/bin/chown servers.servers \"$core\"";
    `$cmd`;
   }
   GDCommon::alert_mail("$GDCommon::hostname core watch","Too many cores in server $node");
  }
  else
  {
   foreach $core (@cores)
   {
    chomp($core);
    $cmd="/bin/chown servers.servers \"$core\"";
    `$cmd`;
   }
  }
  chdir("/opt/servers/bin");
 }
}

