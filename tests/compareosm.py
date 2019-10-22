from xml.etree.ElementTree import parse

def CompareAttribs(n1, n2):
	if 'visible' in n1.attrib: del n1.attrib['visible']
	if 'visible' in n2.attrib: del n2.attrib['visible']
	if 'changeset' not in n1.attrib: n1.attrib['changeset'] = '0'
	if 'changeset' not in n2.attrib: n2.attrib['changeset'] = '0'

	if len(n1.attrib) != len(n2.attrib):
		print ("Different number of attributes")

	for attr in n1.attrib:
		if attr not in n2.attrib:
			print ("Missing attribute", attr, n1.attrib[attr])
			continue
		
		if attr in ['lat', 'lon', 'minlat', 'maxlat', 'minlon', 'maxlon']:
			diff = float(n1.attrib[attr]) - float(n2.attrib[attr])
			if abs(diff) > 2e-6:
				print ("Attrib", attr, "differs by", diff)

		elif n1.attrib[attr] != n2.attrib[attr]:
			print ("Attrib", attr, " don't match", n1.attrib[attr], n2.attrib[attr])

def GetTags(n1):
	out = {}
	for t in n1.findall('tag'):
		out[t.attrib['k']] = t.attrib['v']
	return out

def GetRefs(n1):
	out = []
	for t in n1.findall('nd'):
		out.append(t.attrib['ref'])
	return out

def GetMembers(n1):
	out = []
	for t in n1.findall('member'):
		out.append((t.attrib['type'], t.attrib['ref'], t.attrib['role']))
	return out

def CompareNode(n1, n2, depth=0):
	if n1.tag != n2.tag:
		print ("Element tags don't match")
	if depth == 0 and n1.attrib['version'] != n2.attrib['version']:
		print ("Versions don't match", n1.attrib, n2.attrib)
	if len(n1) != len(n2):
		print ("Number of children don't match")

	if depth == 0:
		for n1c, n2c in zip(n1, n2):
			CompareNode(n1c, n2c, depth+1)

	if depth == 1:
		CompareAttribs(n1, n2)

		n1t = GetTags(n1)
		n2t = GetTags(n2)
		if n1t != n2t:
			print (n1.tag, n1.attrib['id'], "tags don't match")	
			print (n1t, n2t)

		n1r = GetRefs(n1)
		n2r = GetRefs(n2)
		if n1r != n2r:
			print ("Refs don't match")		

		n1m = GetMembers(n1)
		n2m = GetMembers(n2)
		if n1m != n2m:
			print ("Members don't match")		

if __name__=="__main__":
	t1 = parse("../malta-latest.osm")
	t2 = parse("../sample2.osm")

	CompareNode(t1.getroot(), t2.getroot())


