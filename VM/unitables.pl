#!/usr/bin/perl
use strict;
use warnings;

require v5.12.1;

use constant MINRANGE => 26;        # The minimum length of a range
use constant PRINT_CATEGORIES => 0; # Whether to also emit an enum of Unicode categories
use constant PRINT_CODEPOINTS => 0; # Whether to print a comment with each entry that contains the codepoint of that entry
use constant SILENT_RANGES => 1;    # If false, prints comments whenever a range is created

my $unifile = 'UnicodeData.txt';
my $outfile = $ARGV[0] // 'unicode.data.cpp';

my %categories = (
	TOP_MASK => ["TOP_CATEGORY_MASK", 0xf0],
	SUB_MASK => ["SUB_CATEGORY_MASK", 0x0f],
	L  => ["LETTER          ", 0x10],
	Lu => ["LETTER_UPPERCASE", 0x11],
	Ll => ["LETTER_LOWERCASE", 0x12],
	Lt => ["LETTER_TITLECASE", 0x13],
	Lm => ["LETTER_MODIFIER ", 0x14],
	Lo => ["LETTER_OTHER    ", 0x15],

	M  => ["MARK            ", 0x20],
	Mn => ["MARK_NONSPACING ", 0x21],
	Mc => ["MARK_SPACING    ", 0x22],
	Me => ["MARK_ENCLOSING  ", 0x23],

	N  => ["NUMBER          ", 0x30],
	Nd => ["NUMBER_DECIMAL  ", 0x31],
	Nl => ["NUMBER_LETTER   ", 0x32],
	No => ["NUMBER_OTHER    ", 0x33],

	P  => ["PUNCT           ", 0x40],
	Pc => ["PUNCT_CONNECTOR ", 0x41],
	Pd => ["PUNCT_DASH      ", 0x42],
	Ps => ["PUNCT_OPEN      ", 0x43],
	Pe => ["PUNCT_CLOSE     ", 0x44],
	Pi => ["PUNCT_INITIAL   ", 0x45],
	Pf => ["PUNCT_FINAL     ", 0x46],
	Po => ["PUNCT_OTHER     ", 0x47],

	S  => ["SYMBOL          ", 0x50],
	Sm => ["SYMBOL_MATH     ", 0x51],
	Sc => ["SYMBOL_CURRENCY ", 0x52],
	Sk => ["SYMBOL_MODIFIER ", 0x53],
	So => ["SYMBOL_OTHER    ", 0x54],

	Z  => ["SEPARATOR       ", 0x60],
	Zs => ["SEPARATOR_SPACE ", 0x61],
	Zl => ["SEPARATOR_LINE  ", 0x62],
	Zp => ["SEPARATOR_PARAGRAPH", 0x63],

	C  => ["OTHER           ", 0x70],
	Cc => ["CONTROL         ", 0x71],
	Cf => ["FORMAT          ", 0x72],
	Cs => ["SURROGATE       ", 0x73],
	Co => ["PRIVATE_USE     ", 0x74],
	Cn => ["UNASSIGNED      ", 0x75],
);

open my $uni, '<', $unifile or die "Could not open '$unifile': $!";
open my $out, '>', $outfile or die "Could not open '$outfile': $!";

sub print_data;
sub add_range;
sub add_cased_char;

my $range;
my $last_charcode = 0;
sub process_line($) {
	chomp $_[0];
	my @fields = split /;/, $_[0];

	my ($charcode, $name, $category, $upper, $lower) = (@fields[0..2], @fields[12..13]);
	# If a character lacks these fields, it's a candidate for
	# combination with other adjacent characters in the same
	# category, thus forming a range of characters.
	# NOTE: Adjacent characters only! If the characters are NOT
	# adjacent, then there must be unassigned characters between
	# them. In that case, they belong to different ranges.
	my $is_cased = $upper || $lower;

	$upper = $charcode unless $upper;
	$lower = $charcode unless $lower;

	($charcode, $upper, $lower) = map { hex } $charcode, $upper, $lower;

	if (defined $range) {
		local $1;
		if ($range->{cat} eq $categories{$category}[0] && $range->{end} == $charcode - 1 || # same category, adjacent codepoints
			$name =~ /^<([^,]+), Last>$/) { # same range, non-adjacent codepoints
			$range->{end} = $charcode;
		}
		if (defined($1) || $range->{cat} ne $categories{$category}[0] || $range->{end} < $charcode - 1) {
			add_range($1 // "Automatic group", %$range);
			undef $range;
		}
		$last_charcode = $charcode if defined $1; # To prevent the next condition from thinking the range is a bunch of unassigned characters.
	}
	if ($last_charcode < $charcode - 1) { # There is a gap! Gasp!
		add_range("Unassigned", start => $last_charcode + 1, end => $charcode - 1, cat => $categories{Cn}[0]);
	}

	if ($name !~ /, Last>$/ && ($name =~ /, First>$/ || !defined $range)) {
		$range = {start => $charcode, end => $charcode, cat => $categories{$category}[0]};
	}

	add_cased_char $charcode, $upper, $lower if $is_cased;

	$last_charcode = $charcode;
}

#my $line_format = "\t\t\tUC_%s, 0x%04X, 0x%04X, // U+%04X\n";
my @line_formats;
if (PRINT_CODEPOINTS) {
	@line_formats = ("\tUC_%s /*%04X*/,", " UC_%s /*%04X*/,", " UC_%s /*%04X*/,", " UC_%s /*%04X*/,\n");
} else {
	@line_formats = ("\tUC_%s,", " UC_%s,", " UC_%s,", " UC_%s,\n");
}
my $format = 0;
sub print_data {
	printf $out $line_formats[$format], @_;
	$format = ($format + 1) % 4;
}

my @ranges;
my $range_offset = 0;
sub add_range($%) {
	my ($name, %range) = @_;

	my $count = $range{end} - $range{start} + 1;

	if ($count < MINRANGE) {
		print_data $range{cat}, $range{start}++ while $range{start} <= $range{end};
	} else {
		my $last_range = @ranges ? $ranges[-1] : undef;
		if (defined($last_range) && $last_range->{end} == $range{start} - 1 && $last_range->{cat} eq $range{cat}) { # contiguous ranges
			$last_range->{end} = $range{end}; # whee
			$last_range->{offset} += $count;
			$range_offset += $count;
			printf "Combined ranges:  U+%04X to U+%04X  with  U+%04X to U+%04X\n", $last_range->{start}, $last_range->{end}, $range{start}, $range{end};
		} else {
			$range_offset += $count;
			$range{offset} = $range_offset;
			push @ranges, \%range;
		}

		if (!SILENT_RANGES) {
			(my $cat = $range{cat}) =~ s/^\s+|\s+$//g;
			print $out "\n" if $format;
			print $out "\t\t\t// $name ($cat, $count)\n";
			$format = 0; # reset, whoo!
		}
	}
}

my @cased_chars;
sub add_cased_char($$$) {
	push @cased_chars, [@_];
}

print $out <<EOT;
#include "ov_unicode.internal.h"

/***********************************************************/
/*                                                         */
/*                  DO NOT EDIT THIS FILE                  */
/*               THIS FILE IS AUTO-GENERATED               */
/*                                                         */
/*  To re-generate this file, run the unitables.pl script  */
/*  and make sure UnicodeData.txt is up to date.           */
/*                                                         */
/***********************************************************/

EOT

if (PRINT_CATEGORIES) {
	my @sorted_cat_keys = sort { $categories{$a}[1] <=> $categories{$b}[1] } keys %categories;
	print $out "TYPED_ENUM(UnicodeCategory, uint8_t)\n{\n";

	for my $k (@sorted_cat_keys) {
		printf $out "\tUC_%s = 0x%04X,\n", @{$categories{$k}};
	}

	print $out "};\n\n";
}

print $out "const UnicodeCategory CharCategories[] = {\n";

while (<$uni>) {
	process_line($_);
	# U+FFFD is the last assigned codepoint less than U+10000.
	last if $last_charcode == 0xFFFD;
}
add_range("Automatic group", %$range) if defined $range;
add_range("Unassigned", start => 0xFFFE, end => 0xFFFF, cat => $categories{Cn}[0]);
print $out "\n" if $format;

print $out "};\n\nconst uint16_t Ranges[] = {\n";

for my $r (@ranges) {
	printf $out "\tUC_%s, 0x%04X, 0x%04X, %d,\n", $r->{cat}, $r->{start}, $r->{end}, $r->{offset};
}

print $out "};\n\nconst uchar CaseMaps[] = {\n";

my @case_formats = ("\t0x%04X, 0x%04X, 0x%04X,",  " 0x%04X, 0x%04X, 0x%04X,", " 0x%04X, 0x%04X, 0x%04X,\n");
my $cformat = 0;
for my $c (@cased_chars) {
	printf $out $case_formats[$cformat], @$c;
	$cformat = ($cformat + 1) % 3;
}
print $out "\n" if $cformat;

print $out "};\n\n";

print "\n";
print "#define UNI_RANGE_COUNT    ".(scalar @ranges)."\n";
print "#define UNI_CASEMAP_COUNT  ".(scalar @cased_chars)."\n";

# And now we begin processing the wide Unicode characters!
# (That is, characters above U+FFFF.)
# Mmm, surrogate pairs.

@ranges = ();
@cased_chars = ();
$format = $cformat = 0;

print $out "const UnicodeCategory WCharCategories[] = {\n";

$range_offset = 0;
$last_charcode = 0x10000;
while (<$uni>) {
	process_line($_);
	# U+10FFFD is the last assigned codepoint. Ever.
	last if $last_charcode == 0x10FFFD;
}
add_range("Automatic group", %$range) if defined $range;
add_range("Unassigned", start => 0x10FFFE, end => 0x10FFFF, cat => $categories{Cn}[0]);
print $out "\n" if $format;

print $out "};\n\nconst uint32_t WRanges[] = {\n";

for my $r (@ranges) {
	printf $out "\tUC_%s, 0x%06X, 0x%06X, %d,\n", $r->{cat}, $r->{start}, $r->{end}, $r->{offset};
}

print $out "};\n\nconst wuchar WCaseMaps[] = {\n";

@case_formats = ("\t0x%06X, 0x%06X, 0x%06X,",  " 0x%06X, 0x%06X, 0x%06X,", " 0x%06X, 0x%06X, 0x%06X,\n");
for my $c (@cased_chars) {
	printf $out $case_formats[$cformat], @$c;
	$cformat = ($cformat + 1) % 3;
}
print $out "\n" if $cformat;

print $out "};";

print "\n";
print "#define UNI_WRANGE_COUNT   ".(scalar @ranges)."\n";
print "#define UNI_WCASEMAP_COUNT ".(scalar @cased_chars)."\n";

close $uni;
close $out;