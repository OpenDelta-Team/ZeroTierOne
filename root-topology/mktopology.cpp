#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <map>

#include "../osdep/OSUtils.hpp"
#include "../node/Identity.hpp"
#include "../node/Dictionary.hpp"

using namespace ZeroTier;

int main(int argc,char **argv)
{
	std::string buf;

	// Read root-topology-authority.secret signing authority, must be symlinked and online
	Identity topologyAuthority;
	if (OSUtils::readFile("root-topology-authority.secret",buf))
		topologyAuthority.fromString(buf);
	else std::cerr << "Warning: root-topology-authority.secret not found, creating unsigned topology." << std::endl;

	Dictionary topology;

	// Read template.dict to populate default fields in root topology
	// if this file exists. Otherwise we just start empty.
	buf.clear();
	if (OSUtils::readFile("template.dict",buf))
		topology.fromString(buf);

	// Read all entries in rootservers/ that correspond to rootserver entry dictionaries
	// and add them to topology under rootservers/ subkey.
	Dictionary rootservers;
	std::vector<std::string> rootserverDictionaries(OSUtils::listDirectory("rootservers"));
	for(std::vector<std::string>::const_iterator sn(rootserverDictionaries.begin());sn!=rootserverDictionaries.end();++sn) {
		if (sn->length() == 10) {
			buf.clear();
			if (!OSUtils::readFile((std::string("rootservers/")+(*sn)).c_str(),buf)) {
				std::cerr << "Cannot read rootservers/" << *sn << std::endl;
				return 1;
			}
			rootservers[*sn] = buf;
		}
	}
	topology["rootservers"] = rootservers.toString();

	if ((topologyAuthority)&&(topologyAuthority.hasPrivate())) {
		// Sign topology with root-topology-authority.secret
		if (!topology.sign(topologyAuthority,OSUtils::now())) {
			std::cerr << "Unable to sign!" << std::endl;
			return 1;
		}

		// Test signature to make sure signing worked
		Dictionary test(topology.toString());
		if (!test.verify(topologyAuthority)) {
			std::cerr << "Test verification of signed dictionary failed!" << std::endl;
			return 1;
		}
	}

	// Output to stdout
	std::cout << topology.toString();

	return 0;
}
