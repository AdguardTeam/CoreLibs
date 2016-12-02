package com.adguard.http.proxy;

import com.adguard.http.parser.HttpMessage;
import org.junit.Test;

/**
 * Created by s.fionov on 24.11.16.
 */
public class TestUrlStrip {
	@Test
	public void testUrlStrip() {
		System.loadLibrary("httpparser");
		try (HttpMessage message = HttpMessage.create()) {
			String LJ_URL = "http://l-stat.livejournal.net/??lj-basestrap.css,lj-basestrap-app.css,flatbutton.css,svg/flaticon.css,svg/headerextra.css,lj_base-journal.css,widgets/threeposts.css,widgets/likes.css,updateform_v3.css,commentmanage_v3.css,widgets/filter-settings.css,widgets/login.css,journalpromo/journalpromo_v3.css,widgets/calendar.css,medius/mainpage/discoverytimes.css,ljtimes/ctrl.css,msgsystem.css?v=1479804391";
			message.setUrl(LJ_URL);
			String host = HttpProxyServer.stripHostFromRequestUrl(message);
			System.out.println(message.getUrl());
		}
	}

}
