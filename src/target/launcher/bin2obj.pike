#! /usr/local/bin/pike -DEMACS_MODE="-*-Pike-*-"

constant options = ({
  ({ "outfile",  Getopt.HAS_ARG, ({"-o","--output"})}),
  ({ "writable", Getopt.NO_ARG,  ({"-w","--writable"})}),
  ({ "align",    Getopt.HAS_ARG, ({"-a","--align"})}),
  ({ "help",     Getopt.NO_ARG,  ({"-h","--help"})}),
});

int main(int argc, array(string) argv)
{
  string outfile = "a.out";
  string section = ".rodata";
  int align = 2;

  foreach(Getopt.find_all_options(argv, options), mixed option)
    switch(option[0]) {
     case "outfile": outfile=(string)option[1]; break;
     case "writable": section=".data"; break;
     case "align": align=(int)option[1]; break;
     case "help":
       werror("Usage: "+argv[0]+
	      " [-o outfile] [-w] [-a align] label [file]\n");
       return 1;
    }

  argv = Getopt.get_args(argv);

  if(sizeof(argv)<2 || sizeof(argv)>3) {
    werror("Illegal arguments.\n");
    return 1;
  }

  Stdio.File input;

  if(sizeof(argv)>2) {
    input = Stdio.File();
    if(!input->open(argv[-1], "r")) {
      werror("%s: %s\n", argv[-1], strerror(input->errno()));
      return 1;
    }
  } else input = Stdio.File("stdin");

  Stdio.File pipe = Stdio.File();
  Stdio.File p2 = pipe->pipe();
  Process.Process process = Process.Process(({"sh-elf-as", "-little", "-o", outfile}),
					    (["stdin":p2]));

  p2->close();

  pipe->write(sprintf("\n\t.section %s\n\n\t.align %d\n\n"
		      "\t.globl %s,%s_end\n%s:\n",
		      section, align, argv[1], argv[1], argv[1]));

  string buf;
  while((buf = input->read(65536)) && sizeof(buf))
    foreach(buf/32.0, string s)
      pipe->write(sprintf("\t.byte 0x%02x%{,0x%02x%}\n", s[0], (array)s[1..]));
 
  pipe->write(sprintf("\n%s_end:\n", argv[1]));
  pipe->write("\n\t.end\n\n");
  pipe->close();

  return process->wait();
}
