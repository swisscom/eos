package com.swisscom.eos.data;

import com.swisscom.eos.event.EosError;

public interface IEosDataFeed {
	public EosError enable();
	public EosError disable();
}
