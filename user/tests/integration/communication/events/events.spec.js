suite('Cloud events');

platform('gen2', 'gen3');

test('particle_publish_publishes_an_event', async function() {
	const data = await this.particle.receiveEvent('my_event');
	expect(data).to.equal('event data');
});

test('max_event_data_size', async function() {
	// Original 1500-character string used in the application code
	const str = 'eI568Df9nXQmUyaDeNE7A4pZnrcdaAxetam6QYQe3lXFwzN3A6ZO2VGutxVBbIWc8EyrqFMtzKByspno2vL1bGB9H6btc5GWysJZ3XLa3paAmAG4P3UZcbg4NuSRTSEr2YsDMTIEF2lSdd51YR0BPsbcEiQN29ufOpfEHXqK7LfJ3lfEMySnl0iX3ajaQ9rlLsKF4vhSoLFQDp3SRAmzfHhLCDHqVFDT9o8I4Ac5ER6cPl5k8wucWJqxQVWCHB2jdrtSX3WNX8Uq14mAuS4L4s2SeP6UlCcWXrzV9AAuBeTON9Jw7Lbe09F7Ijz0KxIPlwnVZDqXV09GbxKXIOA41E1ZeR9Cg23vozKZZzn2cWeeYtJmRi5Evmwmjus72XQM1W7KGZABrQbzSZawK0pRk9Cp7kl2uy39IjxL6ev3nlC8EA2DE7zi1DJHW7bJceUvFevQcHjWHU5FNKx7m48SG2046PDxxl0vnkXQ6hompl04RFmjUnIgEfIT9XZCkes5lPa8T2V8Ueo7aDfPBYSZOX35XBCczj6nXZ9oxVqn9zxH5NrLcmeDsLop77PVmdJles0CWEAAr5zNVOxIETN2jJcksLXRfQ1pESo9YLaBTyjSuDRQqMenYwuv2qFFnEbaZCMqBQRvE4ql0Oo6K9rXKdfO5G8b9c9jSI4g56f1DAiv7iWU99NdMUMVFt2LmYZsT0azi6MztjRsbtVRG2thZUqAhaPuhvZd0Efbd5H01oUN2CIsh9NiMdEkG5ouSMVaLGjIuvfDeFnlKjL7wSvmNauWYQY021dCKfpJCx0Q7XRB9kFDWZLcew61CmCHsEctM4JldvVhKLdWcnKFDttz3CfbFgtkGBVPWSW0hOwA2e5SLNwHyyJyJXNsicFxMpelYlVAhFjSR8nXe0cJqylvmKYUQ85H2Qet4kehs4boQLIqTHeDoDy1ITDbNVnv3PWzbna5kmEiBhyRw4kn6Di1a6r7uamd5fgFAGURi9LYCp3wAuw6PbYpq8rFXFFzkOUniI3q5c1bLDFxRS4zxNOuH31W819DZGM57zimuZ8YeEfAljxmSOeUWQQdlJjZbjgvERF1Dlexe4nROXyDOadc4qlznOKL0u2ttG0hCVPHMXG4s4uP8YLXJMhyNZod6mkdW9R42aWAsJgDMZZnuU7J7HJL9OpOZXPDCl1l2wOlPCyUtVQzG7PD1Db0dIaTMe9YnFtNAPPxAD4JQXNKMkmWRrhVE2VuJlNvokoCZp9pBDYBFJEPHOYWZI93gsR2tdSIa7YQslZRykJRAF90xlBfNvljN9yR64g7Q1IKCbGwr59H2I5WFEHruiIFJpPs9QQOYxlgq9juAJ9GyfmpEwuvF6n49Bi34v9dQGwt5ZMRFB6HgoRTb9PaLCp4e0Ns7zYYY2rWwESeZnPsqADsFFG3pxsisIn8pjLdAlJrAdMiyUGaIvi7Vj6uFmClZMI8i39pnWXfJbUSJtofdeCthZD2awxZJMjC';
	const dev = this.particle.devices[0];
	let maxLen = dev.platform.is('gen2') ? 864 : 1024; // See MAX_EVENT_DATA_LENGTH in protocol_defs.h
	const data = await this.particle.receiveEvent('my_event');
	if (dev.platform.is('boron') || dev.platform.is('bsom')) {
		if (data.length < maxLen) {
			// This is most likely an LTE Boron or B SoM with some old modem
			// firmware version that crashes with IP packets over a certain size.
			// This value is re-calculated based on MTU.
			// NOTE: this adjusted size also depends on the size of the event name
			maxLen = 838;
		}
	}
	expect(data).to.equal(str.slice(0, maxLen));
});
