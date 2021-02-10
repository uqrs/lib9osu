# verify.awk: verify semantic contents of two beatmap files
# rely on this if diffing the output alone is inadequate.
#
# cat input.osu | gawk -v cmp=osu9output.osu -f verify.awk
#
# i will not be held liable for any brain damage resulting from
# use or modification of this script
BEGIN {
	if (!cmp)
		exit 1;
}

function tosection(s)
{
	close(cmp);
	while ((getline ln <cmp) > 0) {
		gsub(/\r/,"",ln);
		if (ln == s)
			return 1;
	}

	return 0;
}

function hitsampcmp(s1, s2)
{
	split(s1, samp1, ":");
	split(s2, samp2, ":");

	for (j in samp2) {
		if ((j in samp1) && samp1[j] != samp2[j])
			return 0
	}

	return 1
}

function differ(s1, s2)
{
	print "> " s1
	print "---"
	print "< " s2
	exit 1;
}

{ split("", csv1); split("", csv2); split("", kv1); split("", kv2); split("", samp1); split("", samp2); }
{ gsub(/\r/,"") }
$0 ~ /^\[General\]$/ || $0 ~ /^\[Editor\]$/ || $0 ~ /^\[Colours\]$/ || $0 ~ /^\[Events\]$/ {
	tosection($0)

	split("", lines1);
	split("", lines2);
	n=1;
	while (getline > 0 && (getline ln <cmp) > 0) {
		gsub(/\r/,""); gsub(/\r/,"", ln);
		if ($0 ~ /^\[[A-Za-z]*\]$/) break;
		lines1[n] = $0;
		lines2[n++] = ln;
	}

	for (l in lines1) {
		split(lines1[l], kv1, ":");
		gsub(/ *$/, "", kv1[1]);
		gsub(/^ */, "", kv1[2]);

		found=0;
		for (m in lines2) {
			split(lines2[m], kv2, ":");
			gsub(/^ */, "", kv2[2]);
			gsub(/ *$/, "", kv2[1]);
			if (kv1[1] == kv2[1]) {
				found=1;
				ln=lines2[m];
				break;
			}
		}
		if (!found) {
			print "missing " kv1[1];
			exit 1
		}

		val1=""; val2="";
		for (i in kv1) {
			if (i==1) continue;
			val1=val1 kv1[i];
		}
		for (i in kv2) {
			if (i==1) continue;
			val2=val2 kv2[i];
		}

		if (kv1[1] == "StackLeniency") {
			if (kv1[1] != kv2[1] || val1+0 != val2+0)
				differ(lines1[l], ln);
		} else if (kv1[1] != kv2[1] || val1 != val2) differ(lines1[l], ln);
	}
}

$0 ~ /^\[Metadata\]$/ {
	tosection($0)
	while (getline > 0 && (getline ln <cmp) > 0) {
		gsub(/\r/,""); gsub(/\r/,"", ln);
		if ($0 ~ /^\[[A-Za-z]*\]$/) break;
		if ($0 != ln) differ($0, ln);
	}
}

$0 ~ /^\[Difficulty\]$/ {
	tosection($0)
	while (getline > 0 && (getline ln <cmp) > 0) {
		gsub(/\r/,""); gsub(/\r/,"", ln);
		if ($0 ~ /^\[[A-Za-z]*\]$/) break;
		split($0, kv1, ":");
		split(ln, kv2, ":");

		if (kv1[1] != kv2[1] || (kv1[2]+0) != (kv2[2]+0))
			differ($0, ln);
	}
}

$0 ~ /^\[TimingPoints\]$/ {
	tosection($0)
	while (getline > 0 && (getline ln <cmp) > 0) {
		gsub(/\r/,""); gsub(/\r/,"", ln);
		if ($0 ~ /^\[[A-Za-z]*\]$/) break;
		split($0, csv1, ",");
		split(ln, csv2, ",");

		if ($0 == "") continue;

		for (i in csv1) {
			if (csv1[i]+0 != csv2[i]+0)
				differ($0, ln);
		}

		if (!(7 in csv1) && csv2[7] != 1)
			differ($0, ln);
		if (!(8 in csv1) && csv2[8] != 0)
			differ($0, ln);
	}
}

$0 ~ /^\[HitObjects\]$/ {
	tosection($0)
	while (getline > 0 && (getline ln <cmp) > 0) {
		gsub(/\r/,""); gsub(/\r/,"", ln);
		if ($0 ~ /^\[[A-Za-z]*\]$/) break;
		split($0, csv1, ",");
		split(ln, csv2, ",");

		for (i=1; i<=5; i++)
			if (csv1[i]+0 != csv2[i]+0)
				differ($0, ln);

		type = and(csv1[4], 11);

		if (type == 1) {
			if (6 in csv1) {
				if (!hitsampcmp(csv1[6], csv2[6]))
					differ($0, ln);
			} else {
				if (!hitsampcmp("0:0:0:0:", csv2[6]))
					differ($0, ln);
			}
		} else if (type == 2) {
			for (i=6; i<=9; i++)
				if (csv1[i] != csv2[i])
					differ($0, ln);

			if (10 in csv1) {
				split(csv1[10], esamp1, "|");
				split(csv2[10], esamp2, "|");
				for (k in esamp1) {
					split(esamp1[k], eduo1, ":");
					split(esamp2[k], eduo2, ":");
					if (eduo1[1] != eduo2[1])
						differ($0, ln);
					if (!(2 in eduo1) && eduo2[2] != "0")
						differ($0, ln);
					else if (eduo1[2] != eduo2[2])
						differ($0, ln);
				}
			}

			if (11 in csv1) {
				if (!hitsampcmp(csv1[11], csv2[11]))
					differ($0, ln);
			 } else if (11 in csv2) {
				if (!hitsampcmp("0:0:0:0:", csv2[11]))
					differ($0, ln);
			}
		} else if (type == 8) {
			for (i=1; i<=6; i++)
				if (csv1[i] != csv2[i])
					differ($0, ln);

			if (7 in csv1) {
				if (!hitsampcmp(csv1[7], csv2[7]))
					differ($0, ln);
			 } else {
				if (!hitsampcmp("0:0:0:0:", csv2[7]))
					differ($0, ln);
			}
		} else {
			print "What is wrong with you?"
			exit 69;
		}
	}
}
