#!/usr/bin/perl

use lib "/home/servers/adm-scripts";
use GDCommon;

@files=</media/ephemeral0/logic_logs/*.log>;

foreach $file (@files)
{
	if(-s $file>1000000000)
	{
		$cmd="/bin/rm -f $file";
		`$cmd`;
	        $action_desc="EGS $GDCommon:hostname game log $file too big, forced remove!";
	        GDCommon::alert_mail("$GDCommon::hostname game log too big!",$action_desc);
	        GDCommon::alert_sms($action_desc);
	}
}

