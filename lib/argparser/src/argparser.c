/**
 * argparser – command line argument parser library
 * 
 * Copyright © 2013  Mattias Andrée (maandree@member.fsf.org)
 * 
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "argparser.h"


/* Code style constants */
#define true  1
#define false 0
#define null  0

/* Constants */
#define ARGUMENTLESS  0
#define ARGUMENTED    1
#define OPTARGUMENTED 2
#define VARIADIC      3

/* Prototype for static functions */
static const char* args__abbreviations(const char* argument);
static void _sort(const char** list, size_t count, const char** temp);
static void sort(const char** list, size_t count);
static void _sort_ptr(void** list, size_t count, void** temp);
static void sort_ptr(void** list, size_t count);
static int cmp(const char* a, const char* b);
static void map_init(args_Map* map);
static void* map_get(args_Map* map, const char* key);
static void map_put(args_Map* map, const char* key, void* value);
static void _map_free(void** level, int has_value);
static void** map_free(args_Map* map);
static void noop_2(const char* used, const char* std);
static void noop_3(const char* used, const char* std, char* value);
static int default_stickless(const char* argument);


/**
 * Whether the Linux VT is being used
 */
static int args_linuxvt;

/**
 * Whether to use single dash/plus long options
 */
static int args_alternative;

/**
 * Queue of objects that needs to be freed on dispose
 */
static void** args_freequeue;

/**
 * The number of elements in `args_freequeue`
 */
static ssize_t args_freeptr;

/**
 * Options, in order
 */
static args_Option* args_options;

/**
 * Number of elements in `args_options`
 */
static size_t args_options_count;

/**
 * Number of elements for which `args_options` is allocated
 */
static size_t args_options_size;

/**
 * Option map
 */
static args_Map args_optmap;

/**
 * Parsed arguments, a map from option to arguments, with one `null` element per argumentless use
 */
static args_Map args_opts;

/**
 * Used in `map_free` and `_map_free` to store found values that can be freed
 */
static void** args_map_values;

/**
 * The number of elements in `args_map_values`
 */
static size_t args_map_values_ptr;

/**
 * The size of `args_map_values`
 */
static size_t args_map_values_size;



/**
 * Initialiser.
 * The short description is printed on same line as the program name
 * 
 * @param  description      Short, single-line, description of the program
 * @param  usage            Formated, multi-line, usage text, may be `null`
 * @param  longdescription  Long, multi-line, description of the program, may be `null`
 * @param  program          The name of the program, `null` for automatic
 * @param  usestderr        Whether to use stderr instead of stdout
 * @param  alternative      Whether to use single dash/plus long options
 * @param  abbreviations    Abbreviated option expander, `null` for disabled
 */
void args_init(const char* description, const char* usage, const char* longdescription, const char* program, int usestderr, int alternative, const char* (*abbreviations)(const char*, const char**, size_t))
{
  char* term = getenv("TERM");
  args_linuxvt = 0;
  if (term != null)
    if (*(term + 0) == 'l')
      if (*(term + 1) == 'i')
	if (*(term + 2) == 'n')
	  if (*(term + 3) == 'u')
	    if (*(term + 4) == 'x')
	      if (*(term + 5) == 0)
		args_linuxvt = 1;
  args_program = program == null ? args_parent_name(0) : strdup(program);
  if (args_program == null)
    args_program = strdup("?");
  args_description = description;
  args_usage = usage;
  args_longdescription = longdescription;
  args_out_fd = usestderr ? 2 : 1;
  args_out = usestderr ? stderr : stdout;
  args_alternative = alternative;
  args_arguments_count = args_unrecognised_count = args_files_count = 0;
  args_files = args_arguments = null;
  args_message = null;
  args_freequeue = null;
  args_freeptr = 0;
  args_options_count = 0;
  args_options_size = 64;
  args_options = (args_Option*)malloc(args_options_size * sizeof(args_Option));
  map_init(&args_optmap);
  map_init(&args_opts);
  args_abbreviations = abbreviations;
}


/**
 * Disposes of all resources, run this when you are done
 */
void args_dispose(void)
{
  if (args_files != null)
    free(args_files);
  if (args_message != null)
    free(args_message);
  if (args_program != null)
    free(args_program);
  if (args_options != null)
    {
      size_t i;
      for (i = 0; i < args_options_count; i++)
	free((*(args_options + i)).alternatives);
      free(args_options);
    }
  
  args_files = null;
  args_message = null;
  args_program = null;
  args_options = null;
  
  if (args_freequeue != null)
    {
      for (args_freeptr -= 1; args_freeptr >= 0; args_freeptr--)
	free(*(args_freequeue + args_freeptr));
      free(args_freequeue);
      args_freequeue = null;
    }
  
  free(map_free(&args_optmap));
  
  {
    void** freethis = map_free(&args_opts);
    size_t i = 0, count = 0, last = 0, new, size = 128;
    void** values = (void**)malloc(size * sizeof(void*));
    for (; *(freethis + i); i++)
      {
	args_Array* value = *(freethis + i);
	if (count == size)
	  values = (void**)realloc(values, (size <<= 1) * sizeof(void*));
	*(values + count++) = value->values;
	free(value);
      }
    sort_ptr(values, count);
    for (i = 0; i < count; i++)
      {
	new = (size_t)(void*)*(values + i);
	if (new != last)
	  {
	    last = new;
	    free(*(values + i));
	  }
      }
    free(values);
    free(freethis);
  }
}


/**
 * The standard abbrevation expander
 * 
 * @param   argument  The option that not recognised
 * @param   options   All recognised options
 * @param   count     The number of elements in `options`
 * @return            The only possible expansion, otherwise `null`
 */
const char* args_standard_abbreviations(const char* argument, const char** options, size_t count)
{
  const char* rc = null;
  size_t i;
  for (i = 0; i < count; i++)
    {
      size_t match = 0;
      const char* opt = *(options + i);
      while (*(argument + match) && (*(opt + match) == *(argument + match)))
	match++;
      if (*(argument + match) == 0)
	{
	  if (rc)
	    return null;
	  rc = opt;
	}
    }
  return rc;
}


/**
 * Abbreviated option expansion function
 * 
 * @param   argument  The option that is not recognised
 * @return            The only alternative, or `null`
 */
const char* args__abbreviations(const char* argument)
{
  if (args_abbreviations == null)
    return null;
  return args_abbreviations(argument, args_get_optmap(), args_get_optmap_count());
}


/**
 * Creates, but does not add, a option that takes no arguments
 * 
 * @param   trigger          Function to invoke when the option is used, with the used option and the standard option
 * @param   standard         The index of the standard alternative name
 * @param   alternatives...  The alternative names, end with `null`
 * @return                   The created option
 */
args_Option args_new_argumentless(void (*trigger)(const char*, const char*), ssize_t standard, const char* alternatives, ...)
{
  size_t count = 1;
  args_Option rc;
  va_list args, cp;
  size_t i;
  
  va_start(args, alternatives);
  
  va_copy(cp, args); /* va_copy(dest, src) */
  while (va_arg(cp, char*) != null)
    count++;
  va_end(cp);
  
  rc.type = ARGUMENTLESS;
  rc.help = null;
  rc.argument = "NOTHING";
  rc.alternatives_count = count;
  rc.trigger = trigger == null ? noop_2 : trigger;
  
  rc.alternatives = (const char**)malloc(count * sizeof(const char*));
  *(rc.alternatives) = alternatives;
  for (i = 1; i < count; i++)
    *(rc.alternatives + i) = va_arg(args, const char*);
  va_end(args);
  if (standard < 0)
    standard += (ssize_t)(rc.alternatives_count);
  rc.standard = *(rc.alternatives + standard);
  return rc;
}

/**
 * Creates, but does not add, a option that takes one argument per use
 * 
 * @param   trigger          Function to invoke when the option is used, with the used option, the standard option and the used value
 * @param   argument         The new of the argument
 * @param   standard         The index of the standard alternative name
 * @param   alternatives...  The alternative names, end with `null`
 * @return                   The created option
 */
args_Option args_new_argumented(void (*trigger)(const char*, const char*, char*), const char* argument, ssize_t standard, const char* alternatives, ...)
{
  size_t count = 1;
  args_Option rc;
  va_list args, cp;
  size_t i;
  
  va_start(args, alternatives);
  
  va_copy(cp, args); /* va_copy(dest, src) */
  while (va_arg(cp, char*) != null)
    count++;
  va_end(cp);
  
  rc.type = ARGUMENTED;
  rc.help = null;
  rc.argument = argument == null ? "ARG" : argument;
  rc.alternatives_count = count;
  rc.triggerv = trigger == null ? noop_3 : trigger;
  
  rc.alternatives = (const char**)malloc(count * sizeof(const char*));
  *(rc.alternatives) = alternatives;
  for (i = 1; i < count; i++)
    *(rc.alternatives + i) = va_arg(args, const char*);
  va_end(args);
  if (standard < 0)
    standard += (ssize_t)(rc.alternatives_count);
  rc.standard = *(rc.alternatives + standard);
  return rc;
}

/**
 * Creates, but does not add, a option that optionally takes one argument per use
 * 
 * @param   stickless        Should return true if the (feed) next argument can used for the argument without being sticky
 * @param   trigger          Function to invoke when the option is used, with the used option, the standard option and the used value
 * @param   argument         The new of the argument
 * @param   standard         The index of the standard alternative name
 * @param   alternatives...  The alternative names, end with `null`
 * @return                   The created option
 */
args_Option args_new_optargumented(int (*stickless)(const char*), void (*trigger)(const char*, const char*, char*), const char* argument, ssize_t standard, const char* alternatives, ...)
{
  size_t count = 1;
  args_Option rc;
  va_list args, cp;
  size_t i;
  
  va_start(args, alternatives);
  
  va_copy(cp, args); /* va_copy(dest, src) */
  while (va_arg(cp, char*) != null)
    count++;
  va_end(cp);
  
  rc.type = OPTARGUMENTED;
  rc.help = null;
  rc.argument = argument == null ? "ARG" : argument;
  rc.alternatives_count = count;
  rc.triggerv = trigger == null ? noop_3 : trigger;
  rc.stickless = stickless == null ? default_stickless : stickless;
  
  rc.alternatives = (const char**)malloc(count * sizeof(const char*));
  *(rc.alternatives) = alternatives;
  for (i = 1; i < count; i++)
    *(rc.alternatives + i) = va_arg(args, const char*);
  va_end(args);
  if (standard < 0)
    standard += (ssize_t)(rc.alternatives_count);
  rc.standard = *(rc.alternatives + standard);
  return rc;
}

/**
 * Creates, but does not add, a option that takes all following arguments
 * 
 * @param   trigger          Function to invoke when the option is used, with the used option and the standard option
 * @param   argument         The new of the argument
 * @param   standard         The index of the standard alternative name
 * @param   alternatives...  The alternative names, end with `null`
 * @return                   The created option
 */
args_Option args_new_variadic(void (*trigger)(const char*, const char*), const char* argument, ssize_t standard, const char* alternatives, ...)
{
  size_t count = 1;
  args_Option rc;
  va_list args, cp;
  size_t i;
  
  va_start(args, alternatives);
  
  va_copy(cp, args); /* va_copy(dest, src) */
  while (va_arg(cp, char*) != null)
    count++;
  va_end(cp);
  
  rc.type = VARIADIC;
  rc.help = null;
  rc.argument = argument == null ? "ARG" : argument;
  rc.alternatives_count = count;
  rc.trigger = trigger == null ? noop_2 : trigger;
  
  rc.alternatives = (const char**)malloc(count * sizeof(const char*));
  *(rc.alternatives) = alternatives;
  for (i = 1; i < count; i++)
    *(rc.alternatives + i) = va_arg(args, const char*);
  va_end(args);
  if (standard < 0)
    standard += (ssize_t)(rc.alternatives_count);
  rc.standard = *(rc.alternatives + standard);
  return rc;
}


/**
 * Gets an array of all options
 * 
 * @return  All options
 */
args_Option* args_get_options(void)
{
  return args_options;
}

/**
 * Gets the number of elements in the array returned by `args_get_options`
 * 
 * @return  The number of elements in the array returned by `args_get_options`
 */
size_t args_get_options_count(void)
{
  return args_options_count;
}

/**
 * Gets the option with a specific index
 * 
 * @param   index  The option's index
 * @return         The option
 */
args_Option args_options_get(size_t index)
{
  return *(args_options + index);
}

/**
 * Gets the type of a option with a specific index
 * 
 * @param   index  The option's index
 * @return         The option's type
 */
int args_options_get_type(size_t index)
{
  return (*(args_options + index)).type;
}

/**
 * Gets the number of alternative option names for a option with a specific index
 * 
 * @param   index  The option's index
 * @return         The option's number of alternative option names
 */
size_t args_options_get_alternatives_count(size_t index)
{
  return (*(args_options + index)).alternatives_count;
}

/**
 * Gets the alternative option names for a option with a specific index
 * 
 * @param   index  The option's index
 * @return         The option's alternative option names
 */
const char** args_options_get_alternatives(size_t index)
{
  return (*(args_options + index)).alternatives;
}

/**
 * Gets the argument name for a option with a specific index
 * 
 * @param   index  The option's index
 * @return         The option's argument name
 */
const char* args_options_get_argument(size_t index)
{
  return (*(args_options + index)).argument;
}

/**
 * Gets the standard option name for a option with a specific index
 * 
 * @param   index  The option's index
 * @return         The option's standard option name
 */
const char* args_options_get_standard(size_t index)
{
  return (*(args_options + index)).standard;
}

/**
 * Gets the help text for a option with a specific index
 * 
 * @param   index  The option's index
 * @return         The option's help text
 */
const char* args_options_get_help(size_t index)
{
  return (*(args_options + index)).help;
}


/**
 * Gets the available options
 * 
 * @return  The available options
 */
const char** args_get_opts(void)
{
  return args_opts.keys;
}

/**
 * Gets the number of available options
 * 
 * @return  The number of available options
 */
size_t args_get_opts_count(void)
{
  return args_opts.key_count;
}

/**
 * Gets whether an option is available
 * 
 * @param   name  The option
 * @return        Whether an option is available
 */
int args_opts_contains(const char* name)
{
  return map_get(&args_opts, name) != null;
}

/**
 * Initialise an option
 * 
 * @param  name  The option
 */
void args_opts_new(const char* name)
{
  args_opts_put(name, null);
  args_opts_put_count(name, 0);
}

/**
 * Appends a value to an option
 * 
 * @param  name   The option
 * @param  value  The new value
 */
void args_opts_append(const char* name, char* value)
{
  size_t size = args_opts_get_count(name) + 1;
  char** values = args_opts_get(name);
  if (values == null)
    {
      char** array = (char**)malloc(sizeof(char*));
      *array = value;
      args_opts_put(name, array);
    }
  else
    {
      size_t address = (size_t)(void*)values;
      values = (char**)realloc(values, size * sizeof(char*));
      *(values + size - 1) = value;
      if ((size_t)(void*)values != address)
	args_opts_put(name, values);
    }
  args_opts_put_count(name, size);
}

/**
 * Removes all values from an option
 * 
 * @param  name  The option
 */
void args_opts_clear(const char* name)
{
  char** value = args_opts_get(name);
  if (value != null)
    free(value);
  args_opts_new(name);
}

/**
 * Gets the values for an option
 * 
 * @param   name  The option
 * @return        The values
 */
char** args_opts_get(const char* name)
{
  args_Array* value = (args_Array*)map_get(&args_opts, name);
  if (value == null)
    return null;
  return (char**)value->values;
}

/**
 * Gets the number of values for an option
 * 
 * @param   name  The option
 * @return        The number of values
 */
size_t args_opts_get_count(const char* name)
{
  args_Array* value = (args_Array*)map_get(&args_opts, name);
  if (value == null)
    return 0;
  return value->count;
}

/**
 * Sets the values for an option
 * 
 * @param  name   The option
 * @param  count  The values
 */
void args_opts_put(const char* name, char** values)
{
  args_Array* value = (args_Array*)map_get(&args_opts, name);
  if (value == null)
    {
      value = (args_Array*)malloc(sizeof(args_Array));
      value->values = (void**)values;
      map_put(&args_opts, name, value);
      value->used = false;
    }
  else
    value->values = (void**)values;
}

/**
 * Sets the number of values for an option
 * 
 * @param  name   The option
 * @param  count  The number of values
 */
void args_opts_put_count(const char* name, size_t count)
{
  args_Array* value = (args_Array*)map_get(&args_opts, name);
  if (value == null)
    {
      value = (args_Array*)malloc(sizeof(args_Array));
      value->count = count;
      map_put(&args_opts, name, value);
    }
  else
    value->count = count;
}

/**
 * Checks whether an option is used
 * 
 * @param   name  The option
 * @return        Whether the option is used
 */
int args_opts_used(const char* name)
{
  args_Array* value = (args_Array*)map_get(&args_opts, name);
  if (value == null)
    return false;
  return value->used;
}


/**
 * Gets all alternativ names that exists for all options combined
 * 
 * @return  All alternativ names that exists for all options
 */
const char** args_get_optmap(void)
{
  return args_optmap.keys;
}

/**
 * Gets the number of elements returned by `args_get_optmap`
 * 
 * @return  The number of elements returned by `args_get_optmap`
 */
size_t args_get_optmap_count(void)
{
  return args_optmap.key_count;
}

/**
 * Maps alternative name for a option
 * 
 * @param  name   The option's alternative name
 * @param  index  The option's index
 */
void args_optmap_put(const char* name, size_t index)
{
  map_put(&args_optmap, name, (void*)(index + 1));
}

/**
 * Gets the option with a specific alternative name
 * 
 * @param   name  The option's alternative name
 * @return        The option
 */
args_Option args_optmap_get(const char* name)
{
  return *(args_options + args_optmap_get_index(name));
}

/**
 * Gets the index of a option with a specific alternative name
 * 
 * @param   name  The option's alternative name
 * @return        The option's index, negative if not found
 */
ssize_t args_optmap_get_index(const char* name)
{
  void* ret = map_get(&args_optmap, name);
  return ((ssize_t)ret) - 1;
}

/**
 * Checks whether an options with a specific alternative name exists
 * 
 * @param   name  One of the names of the option
 * @return        Whether the option exists
 */
int args_optmap_contains(const char* name)
{
  return args_optmap_get_index(name) >= 0;
}

/**
 * Gets the type of a option with a specific alternative name
 * 
 * @param   name  The option's alternative name
 * @return        The option's type
 */
int args_optmap_get_type(const char* name)
{
  return (*(args_options + args_optmap_get_index(name))).type;
}

/**
 * Gets the standard option name for a option with a specific alternative name
 * 
 * @param   name  The option's alternative name
 * @return        The option's standard option name
 */
const char* args_optmap_get_standard(const char* name)
{
  return (*(args_options + args_optmap_get_index(name))).standard;
}

/**
 * Trigger an option
 * 
 * @param  name   The option's alternative name
 * @param  value  The use value, `null` if argumentless or variadic
 */
void args_optmap_trigger(const char* name, char* value)
{
  args_Option* opt = args_options + args_optmap_get_index(name);
  if (value == null)
    opt->trigger(name, opt->standard);
  else
    opt->triggerv(name, opt->standard, value);
}

/**
 * Trigger an option
 * 
 * @param  name   The option's alternative name
 * @param  value  The use value
 */
void args_optmap_triggerv(const char* name, char* value)
{
  args_Option* opt = args_options + args_optmap_get_index(name);
  opt->triggerv(name, opt->standard, value);
}

/**
 * Evaluate if an argument can be used without being sticky for an optionally argument option
 * 
 * @param   name      The option's alternative name
 * @param   argument  The argument
 * @return            Whether the argument can be used wihout being sticky
 */
int args_optmap_stickless(const char* name, char* argument)
{
  return (args_options + args_optmap_get_index(name))->stickless(argument);
}


/**
 * Adds an option
 * 
 * @param  option  The option
 * @param  help    Help text, multi-line, `null` if hidden
 */
void args_add_option(args_Option option, const char* help)
{
  if (args_options_count == args_options_size)
    args_options = (args_Option*)realloc(args_options, (args_options_size <<= 1) * sizeof(args_Option));
  
  {
    size_t i = 0, n = option.alternatives_count;
    for (; i < n; i++)
      args_optmap_put(*(option.alternatives + i), args_options_count);
    args_opts_put(option.standard, null);
    args_opts_put_count(option.standard, 0);
    *(args_options + args_options_count) = option;
    (*(args_options + args_options_count++)).help = help;
  }
}


/**
 * Gets the name of the parent process
 * 
 * @param   levels  The number of parents to walk, 0 for self, and 1 for direct parent
 * @return          The name of the parent process, `null` if not found
 */
char* args_parent_name(size_t levels)
{
  char pid[22]; /* 6 should be enough, but we want to be future proof */
  ssize_t pid_n = readlink("/proc/self", pid, 21);
  size_t lvl = levels, i, j, cmdsize, off;
  size_t n;
  FILE* is;
  char buf[35];
  char* cmd;
  char* data;
  if (pid_n <= 0)
    return null;
  pid[pid_n] = 0;
  data = (char*)malloc(2048 * sizeof(char));
  while (lvl > 0)
    {
      int found = false;
      i = 0;
      for (j = 0; *("/proc/" + j);  j++)  *(buf + i++) = *("/proc/" + j);
      for (j = 0; *(pid + j);       j++)  *(buf + i++) = *(pid + j);
      for (j = 0; *("/status" + j); j++)  *(buf + i++) = *("/status" + j);
      *(buf + i++) = 0;
      if ((is = fopen(buf, "r")) == null)
	{
	  free(data);
	  return null;
	}
      n = fread(data, 1, 2048, is);
      j = 0;
      for (i = 0; i < (size_t)n; i++)
	{
	  char c = *(data + i);
	  if (c == '\n')
	    {
	      if (j > 5)
		if (*(buf + 0) == 'P')
		  if (*(buf + 1) == 'P')
		    if (*(buf + 2) == 'i')
		      if (*(buf + 3) == 'd')
			if (*(buf + 4) == ':')
			  {
			    i = 5;
			    while ((*(buf + i) == '\t') || (*(buf + i) == ' '))
			      i++;
			    j -= n = i;
			    off = n;
			    for (i = 0; i < j; i++)
			      *(pid + i) = *(buf + off + i);
			    *(pid + j) = 0;
			    lvl--;
			    found = true;
			    break;
			  }
	      j = 0;
	    }
	  else if (j < 35)
	    *(buf + j++) = c;
	}
      fclose(is);
      if (found == false)
	{
	  free(data);
	  return null;
	}
    }
  free(data);
  i = 0;
  for (j = 0; *("/proc/" + j);   j++)  *(buf + i++) = *("/proc/" + j);
  for (j = 0; *(pid + j);        j++)  *(buf + i++) = *(pid + j);
  for (j = 0; *("/cmdline" + j); j++)  *(buf + i++) = *("/cmdline" + j);
  *(buf + i++) = 0;
  if ((is = fopen(buf, "r")) == null)
    return null;
  i = 0;
  n = 0;
  cmd = (char*)malloc((cmdsize = 128) * sizeof(char));
  for (;;)
    {
      n += fread(cmd, 1, 128, is);
      for (; i < (size_t)n; i++)
	if (*(cmd + i) == 0)
	  break;
      if (i == (size_t)n)
	cmd = (char*)realloc(cmd, (cmdsize + 128) * sizeof(char));
      else
	break;
    }
  fclose(is);
  if (*cmd == 0)
    {
      free(cmd);
      cmd = 0;
    }
  return cmd;
}


/**
 * Checks the correctness of the number of used non-option arguments
 * 
 * @param   min  The minimum number of files
 * @return       Whether the usage was correct
 */
int args_test_files_min(size_t min)
{
  return min <= args_files_count;
}


/**
 * Checks the correctness of the number of used non-option arguments
 * 
 * @param   max  The maximum number of files
 * @return       Whether the usage was correct
 */
int args_test_files_max(size_t max)
{
  return args_files_count <= max;
}


/**
 * Checks the correctness of the number of used non-option arguments
 * 
 * @param   min  The minimum number of files
 * @param   max  The maximum number of files
 * @return       Whether the usage was correct
 */
int args_test_files(size_t min, size_t max)
{
  return (min <= args_files_count) && (args_files_count <= max);
}


/**
 * Checks for out of context option usage
 * 
 * @param   allowed        Allowed options, will be sorted
 * @param   allowed_count  The number of elements in `allowed`
 * @return                 Whether only allowed options was used
 */
int args_test_allowed(const char** allowed, size_t allowed_count)
{
  const char** opts;
  const char** a;
  size_t _a, _o, i;
  int rc = true;
  
  sort(allowed, _a = allowed_count);
  opts = args_get_opts();
  sort(opts, _o = args_get_opts_count());
  
  a = allowed + _a;
  
  for (i = 0; i < _o; i++, opts++)
    {
      if ((allowed == a) || (cmp(*opts, *allowed) < 0))
	if (args_opts_used(*opts))
	  {
	    const char* std = args_optmap_get_standard(*opts);
	    fprintf(args_out, "%s: option used out of context: %s", args_program, *opts);
	    if (cmp(std, *opts) != 0)
	      fprintf(args_out, "(%s)", std);
	    fprintf(args_out, "\n");
	    rc = false;
	  }
      while ((allowed != a) && (cmp(*opts, *allowed) > 0))
	allowed++;
    }
  
  return rc;
}


/**
 * Checks for option conflicts
 * 
 * @param   exclusives        Exclusive options, will be sorted
 * @param   exclusives_count  The number of elements in `exclusives`
 * @return                    Whether at most one exclusive option was used
 */
int args_test_exclusiveness(const char** exclusives, size_t exclusives_count)
{
  size_t used_ptr = 0, i = 0;
  const char** used = (const char**)malloc(args_get_opts_count() * sizeof(const char*));
  const char** e;
  const char** o;
  const char** opts;
  const char* std;
  size_t _e, _o;
  
  sort(exclusives, _e = exclusives_count);
  opts = args_get_opts();
  sort(opts, _o = args_get_opts_count());
  
  e = exclusives + _e;
  o = opts + _o;
  
  while ((opts != o) && (exclusives != e))
    {
      while ((opts != o) && (cmp(*opts, *exclusives) > 0))
	opts++;
      while ((exclusives != e) && (cmp(*opts, *exclusives) > 0))
	exclusives++;
      if ((cmp(*opts, *exclusives) == 0) && (args_opts_used(*opts)))
	*(used + used_ptr++) = *opts;
      opts++;
    }
  
  if (used_ptr >= 1)
    {
      fprintf(args_out, "%s: conflicting options:", args_program);
      for (; i < used_ptr; i++)
	{
	  std = args_optmap_get_standard(*(used + i));
	  if (cmp(*(used + i), std) == 0)
	    fprintf(args_out, " %s", *(used + i));
	  else
	    fprintf(args_out, " %s(%s)", *(used + i), std);
	}
      fprintf(args_out, "\n");
      free(used);
      return false;
    }
  
  free(used);
  return true;
}


/**
 * Maps up options that are alternatives to the first alternative for each option
 */
void args_support_alternatives(void)
{
  const char** opts = args_get_optmap();
  size_t n = args_get_optmap_count();
  size_t i;
  
  for (i = 0; i < n; i++)
    {
      const char* alt = *(opts + i);
      const char* std = args_optmap_get_standard(alt);
      args_Array* value_std = (args_Array*)map_get(&args_opts, std);
      args_Array* value_alt;
      
      args_opts_put(alt, (char**)value_std->values);
      value_alt = (args_Array*)map_get(&args_opts, alt);
      value_alt->count = value_std->count;
      value_alt->used = value_std->used;
    }
}


/**
 * Prints a colourful help message
 * 
 * @param  [use_colours]  `0` for no colours, `1` for colours, and `-1` for if not piped
 */
void args_help(long use_colours)
{
  size_t maxfirstlen = 0, count = 0, copts = args_get_options_count();
  const char* dash = args_linuxvt ? "-" : "—";
  char* empty;
  char** lines;
  size_t* lens;
  
  if ((use_colours != 0) && (use_colours != 1))
    use_colours = isatty(args_out_fd);
  
  fprintf(args_out, use_colours ? "\033[01m%s\033[21m %s %s\n" : "%s %s %s\n", args_program, dash, args_description);
  if (args_longdescription != null)
    fprintf(args_out, "%s\n", args_longdescription);
  fprintf(args_out, "\n");
  
  if (args_usage != null)
    {
      size_t n = 0, lines_n = 0, i = 0;
      char* buf;
      fprintf(args_out, use_colours ? "\033[01mUSAGE:\033[21m\n" : "USAGE:\n");
      while (*(args_usage + n))
	if (*(args_usage + n++) == '\n')
	  lines_n++;
      buf = (char*)malloc((n + 2 + lines_n * 7) * sizeof(char));
      *buf++ = '\t';
      while (i < n)
	{
	  *buf++ = *(args_usage + i);
	  if (*(args_usage + i++) == '\n')
	    {
	      *buf++ = ' ';
	      *buf++ = ' ';
	      *buf++ = ' ';
	      *buf++ = ' ';
	      *buf++ = 'o';
	      *buf++ = 'r';
	      *buf++ = '\t';
	    }
	}
      *buf++ = 0;
      buf -= n + 2 + lines_n * 7;
      fprintf(args_out, "%s\n\n", buf);
      free(buf);
    }
  
  {
    size_t i = 0;
    for (i = 0; i < copts; i++)
      {
	if (args_options_get_help(i) == null)
	  continue;
	if (args_options_get_alternatives_count(i) > 1)
	  {
	    size_t n = 0;
	    const char* first = *(args_options_get_alternatives(i));
	    while (*(first + n))
	      n++;
	    if (maxfirstlen < n)
	      maxfirstlen = n;
	  }
      }
  }
  
  empty = (char*)malloc((maxfirstlen + 1) * sizeof(char));
  {
    size_t i;
    for (i = 0; i < maxfirstlen; i++)
      *(empty + i) = ' ';
    *(empty + maxfirstlen) = 0;
  }
  
  fprintf(args_out, use_colours ? "\033[01mSYNOPSIS:\033[21m\n" : "SYNOPSIS:\n");
  lines = (char**)malloc(copts * sizeof(char*));
  lens = (size_t*)malloc(copts * sizeof(size_t));
  {
    char* first_extra = null;
    size_t index = 0, i = 0, n, m, l, j;
    int type;
    for (i = 0; i < copts; i++)
      {
	const char* first;
	const char* last;
	char* line;
	const char* arg;
	if (args_options_get_help(i) == null)
	  continue;
	arg = args_options_get_argument(i);
	first = *(args_options_get_alternatives(i));
	last = *(args_options_get_alternatives(i) + args_options_get_alternatives_count(i) - 1);
	type = args_options_get_type(i);
	if (first == last)
	  {
	    first = empty;
	    first_extra = null;
	  }
	else
	  {
	    n = 0;
	    while (*(first + n))
	      n++;
	    first_extra = empty + n;
	  }
	n = m = 0;
	while (*(last + n))
	  n++;
	if (type != ARGUMENTLESS)
	  while (*(arg + m))
	    m++;
	l = maxfirstlen + 6 + n + m;
	*(lines + count) = line = (char*)malloc((9 + maxfirstlen + 7 + 15 + n + 9 + 6 + m + 5 + 1) * sizeof(char));
	for (j = 0; *((use_colours ? "    \033[02m" : "    ") + j); j++)
	  *line++ = *((use_colours ? "    \033[02m" : "    ") + j);
	for (j = 0; *(first + j); j++)
	  *line++ = *(first + j);
	if (first_extra != null)
	  for (j = 0; *(first_extra + j); j++)
	    *line++ = *(first_extra + j);
	for (j = 0; *((use_colours ? "\033[22m  " : "  ") + j); j++)
	  *line++ = *((use_colours ? "\033[22m  " : "  ") + j);
	if (use_colours)
	  {
	    if ((index++ & 1) == 0)
	      for (j = 0; *("\033[36;01m" + j); j++)
		*line++ = *("\033[36;01m" + j);
	    else
	      for (j = 0; *("\033[34;01m" + j); j++)
		*line++ = *("\033[34;01m" + j);
	  }
	for (j = 0; *(last + j); j++)
	  *line++ = *(last + j);
	if (type == VARIADIC)
	  {
	    for (j = 0; *((use_colours ? " [\033[04m" : " [") + j); j++)
	      *line++ = *((use_colours ? " [\033[04m" : " [") + j);
	    for (j = 0; *(arg + j); j++)
	      *line++ = *(arg + j);
	    for (j = 0; *((use_colours ? "\033[24m...]" : "...]") + j); j++)
	      *line++ = *((use_colours ? "\033[24m...]" : "...]") + j);
	    l += 6;
	  }
	else if (type == OPTARGUMENTED)
	  {
	    for (j = 0; *((use_colours ? " [\033[04m" : " [") + j); j++)
	      *line++ = *((use_colours ? " [\033[04m" : " [") + j);
	    for (j = 0; *(arg + j); j++)
	      *line++ = *(arg + j);
	    for (j = 0; *((use_colours ? "\033[24m]" : "]") + j); j++)
	      *line++ = *((use_colours ? "\033[24m]" : "]") + j);
	    l += 3;
	  }
	else if (type == ARGUMENTED)
	  {
	    if (use_colours)
	      for (j = 0; *(" \033[04m" + j); j++)
		*line++ = *(" \033[04m" + j);
	    else
	      *line++ = ' ';
	    for (j = 0; *(arg + j); j++)
	      *line++ = *(arg + j);
	    if (use_colours)
	      for (j = 0; *("\033[24m" + j); j++)
		*line++ = *("\033[24m" + j);
	    l += 1;
	  }
	*line = 0;
	*(lens + count++) = l;
      }
  }
  
  free(empty);
  
  {
    size_t col = 0, i = 0, index = 0;
    for (; i < count; i++)
      if (col < *(lens + i))
	col = *(lens + i);
    col += 8 - ((col - 4) & 7);
    
    empty = (char*)malloc((col + 1) * sizeof(char));
    for (i = 0; i < col; i++)
      *(empty + i) = ' ';
    *(empty + col) = 0;
    for (i = 0; i < copts; i++)
      {
	int first = true;
	size_t j = 0, jptr = 1;
	const char* colour = (index & 1) == 0 ? "36" : "34";
	const char* help = args_options_get_help(i);
	char* line;
	char* buf;
	char** jumps;
	char c;
	if (help == null)
	  continue;
	fprintf(args_out, "%s%s", line = *(lines + index), empty + *(lens + index));
	free(*(lines + index++));
	while ((c = *(help + j++)))
	  if (c == '\n')
	    jptr++;
	jumps = (char**)malloc(jptr * sizeof(char*));
	*jumps = buf = (char*)malloc(j * sizeof(char));
	j = 0;
	jptr = 1;
	while ((c = *(help + j)))
	  if (c == '\n')
	    {
	      *(buf + j++) = 0;
	      *(jumps + jptr++) = buf + j;
	    }
	  else
	    *(buf + j++) = c;
	*(buf + j) = 0;
	for (j = 0; j < jptr; j++)
	  if (first)
	    {
	      first = false;
	      fprintf(args_out, "%s\033[00m\n", *(jumps + j));
	    }
	  else
	    if (use_colours)
	      fprintf(args_out, "%s\033[%sm%s\033[00m\n", empty, colour, *(jumps + j));
	    else
	      fprintf(args_out, "%s%s\n", empty, *(jumps + j));
	free(buf);
	free(jumps);
      }
  }
  
  free(empty);
  free(lines);
  free(lens);
  fprintf(args_out, "\n");
}


/**
 * Parse arguments
 * 
 * @param   argc  The number of elements in `argv`
 * @param   argv  The command line arguments, it should include the execute file at index 0
 * @return        Whether no unrecognised option is used
 */
int args_parse(int argc, char** argv)
{
  char** argend = argv + argc;
  int dashed = false, tmpdashed = false, rc = true;
  size_t get = 0, dontget = 0;
  size_t argptr = 0, optptr = 0, queuesize = (size_t)argc - 1;
  char* injection = null;
  char** argqueue;
  char** optqueue;
  
  args_freeptr = 0;
  args_unrecognised_count = 0;
  args_arguments_count = (size_t)argc - 1;
  args_arguments = ++argv;
  args_files = (char**)malloc((size_t)(argc - 1) * sizeof(char*));
  
  while (argv != argend)
    {
      char* arg = *argv++;
      if (((*arg == '-') || (*arg == '+')) && (*(arg + 1) != 0))
	if (*arg != *(arg + 1))
	  {
	    size_t i = 1;
	    while (*(arg + i))
	      i++;
	    queuesize += i - 1;
	  }
    }
  
  argv = args_arguments;
  
  argqueue       = (char**)malloc(queuesize * sizeof(char*));
  optqueue       = (char**)malloc(queuesize * sizeof(char*));
  args_freequeue = (void**)malloc(queuesize * sizeof(void*) * 2);
  
  while ((argv != argend) || injection)
    {
      char* arg = injection ? injection : *argv++;
      injection = null;
      if ((get > 0) && (dontget == 0))
	{
	  char* arg_opt = *(optqueue + optptr - get--);
	  long passed = true;
	  if (args_optmap_get_type(arg_opt) == OPTARGUMENTED)
	    if (args_optmap_stickless(arg_opt, arg) == false)
	      {
		passed = false;
		args_optmap_triggerv(arg_opt, null);
		*(argqueue + argptr++) = null;
	      }
	  if (passed)
	    {
	      args_optmap_trigger(arg_opt, arg);
	      *(argqueue + argptr++) = arg;
	      continue;
	    }
	}
      if (tmpdashed)
	{
	  *(args_files + args_files_count++) = arg;
	  tmpdashed = 0;
	}
      else if (dashed)
	*(args_files + args_files_count++) = arg;
      else if ((*arg == '+') && (*(arg + 1) == '+') && (*(arg + 2) == 0))
	tmpdashed = true;
      else if ((*arg == '-') && (*(arg + 1) == '-') && (*(arg + 2) == 0))
	dashed = true;
      else if (((*arg == '-') || (*arg == '+')) && (*(arg + 1) != 0))
	if (args_alternative || (*arg == *(arg + 1)))
	  {
	    size_t eq = 0;
	    int type = 100;
	    if (dontget <= 0)
	      {
		if (args_optmap_contains(arg))
		  type = args_optmap_get_type(arg);
		if (type != ARGUMENTLESS)
		  while (*(arg + eq) && (*(arg + eq) != '='))
		    eq++;
	      }
	    
	    if (dontget > 0)
	      dontget--;
	    else if (type == ARGUMENTLESS)
	      {
		*(optqueue + optptr++) = arg;
		*(argqueue + argptr++) = null;
		args_optmap_trigger(arg, null);
	      }
	    else if (*(arg + eq) == '=')
	      {
		char* arg_opt = (char*)malloc((eq + 1) * sizeof(char));
		size_t i;
		for (i = 0; i < eq; i++)
		  *(arg_opt + i) = *(arg + i);
		*(arg_opt + eq) = 0;

		if (args_optmap_contains(arg_opt) && ((type = args_optmap_get_type(arg_opt)) >= ARGUMENTED))
		  {
		    *(optqueue + optptr++) = arg_opt;
		    *(argqueue + argptr++) = arg + eq + 1;
		    *(args_freequeue + args_freeptr++) = arg_opt;
		    if (type == VARIADIC)
		      {
			dashed = true;
			args_optmap_trigger(arg_opt, null);
		      }
		    else
		      args_optmap_trigger(arg_opt, arg + eq + 1);
		  }
		else
		  {
		    if ((injection = args__abbreviations(arg_opt)))
		      {
			size_t n = 1, j = 0;
			char* _injection;
			for (i = 0; *(injection + i); i++)
			  n++;
			for (i = eq + 1; *(arg + i); i++)
			  n++;
			_injection = (char*)malloc((n + 1) * sizeof(char));
			*(args_freequeue + args_freeptr++) = _injection;
			for (i = 0; *(injection + i); i++)
			  *(_injection + j++) = *(injection + i);
			*((injection = _injection) + j++) = '=';
			for (i = eq + 1; *(arg + i); i++)
			  *(injection + j++) = *(arg + i);
			*(injection + j) = 0;
		      }
		    else
		      {
			if (++args_unrecognised_count <= 5)
			  fprintf(args_out, "%s: warning: unrecognised option %s\n", args_program, arg_opt);
			rc = false;
		      }
		    free(arg_opt);
		  }
	      }
	    else if (type <= OPTARGUMENTED)
	      {
		*(optqueue + optptr++) = arg;
		get++;
	      }
	    else if (type == VARIADIC)
	      {
		dashed = true;
		*(optqueue + optptr++) = arg;
		*(argqueue + argptr++) = null;
		args_optmap_trigger(arg, null);
	      }
	    else
	      if ((injection = args__abbreviations(arg)) == null)
		{
		  if (++args_unrecognised_count <= 5)
		    fprintf(args_out, "%s: warning: unrecognised option %s\n", args_program, arg);
		  rc = false;
		}
	  }
	else
	  {
	    char sign = *arg;
	    size_t i = 1;
	    while (*(arg + i))
	      {
		char* narg = (char*)malloc(3 * sizeof(char));
		*(narg + 0) = sign;
		*(narg + 1) = *(arg + i);
		*(narg + 2) = 0;
		i++;
		if (args_optmap_contains(narg))
		  {
		    long type = args_optmap_get_type(narg);
		    *(args_freequeue + args_freeptr++) = narg;
		    *(optqueue + optptr++) = narg;
		    if (type == ARGUMENTLESS)
		      {
			*(argqueue + argptr++) = null;
			args_optmap_trigger(narg, null);
		      }
		    else if (type < VARIADIC)
		      {
			if (*(arg + i))
			  {
			    *(argqueue + argptr++) = arg + i;
			    args_optmap_trigger(narg, arg + i);
			  }
			else
			  get++;
			break;
		      }
		    else
		      {
			if (*(arg + i))
			  *(argqueue + argptr++) = arg + i;
			dashed = true;
			args_optmap_trigger(narg, null);
			break;
		      }
		  }
		else
		  {
		    if (++args_unrecognised_count <= 5)
		      fprintf(args_out, "%s: warning: unrecognised option %s\n", args_program, arg);
		    rc = false;
		    free(narg);
		  }
	      }
	  }
      else
	*(args_files + args_files_count++) = arg;
    }
  
  {
    size_t i = 0;
    while (i < optptr)
      {
	const char* opt = args_optmap_get_standard(*(optqueue + i));
	char* arg = argptr > i ? *(argqueue + i) : null;
	if (argptr <= i)
	  args_optmap_triggerv(*(optqueue + i), null);
	i++;
	if ((args_optmap_contains(opt) == false) || (args_opts_contains(opt) == false))
	  args_opts_new(opt);
	args_opts_append(opt, arg);
	((args_Array*)map_get(&args_opts, opt))->used = true;
      }
  }
  
  {
    size_t i = 0, j = 0, n = args_get_options_count();
    for (; i < n; i++)
      if (args_options_get_type(i) == VARIADIC)
	{
	  const char* std = args_options_get_standard(i);
	  if (args_opts_used(std))
	    {
	      if (*(args_opts_get(std)) == null)
		args_opts_clear(std);
	      for (j = 0; j < args_files_count; j++)
		args_opts_append(std, *(args_files + j));
	      args_files_count = 0;
	      break;
	    }
	}
  }
  
  free(argqueue);
  free(optqueue);
  
  args_message = null;
  if (args_files_count > 0)
    {
      size_t n = args_files_count, i, j;
      for (i = 0; i < args_files_count; i++)
	{
	  char* file = *(args_files + i);
	  for (j = 0; *(file + j); j++)
	    ;
	  n += j;
	}
      args_message = (char*)malloc(n * sizeof(char));
      n = 0;
      for (i = 0; i < args_files_count; i++)
	{
	  char* file = *(args_files + i);
	  for (j = 0; *(file + j); j++)
	    *(args_message + n++) = *(file + j);
	  *(args_message + n++) = ' ';
	}
      *(args_message + --n) = 0;
    }
  
  if (args_unrecognised_count > 5)
    {
      size_t more = args_unrecognised_count - 5;
      const char* option_s = more == 1 ? "option" : "options";
      fprintf(args_out, "%s: warning: %li more unrecognised %s\n", args_program, more, option_s);
    }
  
  return rc;
}


/**
 * Compare two strings
 * 
 * @param   a  -1 if returned if this sting is the alphabetically lesser one
 * @param   b   1 if returned if this sting is the alphabetically lesser one
 * @return      0 is returned if the two string are identical, other -1 or 1 is returned
 */
static int cmp(const char* a, const char* b)
{
  int c;
  while (*a && *b)
    {
      if ((c = (*a < *b ? -1 : (*a > *b ? 1 : 0))))
	return c;
      a++;
      b++;
    }
  return *a < *b ? -1 : (*a > *b ? 1 : 0);
}

/**
 * Naïve merge sort is best merge sort in C
 * 
 * @param  list   The list to sort from the point that needs sorting
 * @param  count  The number of elements to sort
 * @param  temp   Auxiliary memory
 */
static void _sort(const char** list, size_t count, const char** temp)
{
  if (count > 1)
    {
      size_t i = 0, a = count >> 1;
      size_t j = a, b = count - a;
      _sort(list + 0, a, temp + 0);
      _sort(list + a, b, temp + a);
      b += a;
      while ((i < a) && (j < b))
	{
          int c = cmp(*(temp + i), *(temp + j));
          if (c <= 0)
            *list++ = *(temp + i++);
          else
            *list++ = *(temp + j++);
	}
      while (i < a)
	*list++ = *(temp + i++);
      while (j < b)
	*list++ = *(temp + j++);
      list -= count;
      for (i = 0; i < count; i++)
	*(temp + i) = *(list + i);
    }
  else if (count == 1)
    *temp = *list;
}

/**
 * Naïve merge sort is best merge sort in C
 * 
 * @param  list   The list to sort
 * @param  count  The number of elements to sort
 */
static void sort(const char** list, size_t count)
{
  const char** temp = (const char**)malloc(count * sizeof(const char*));
  _sort(list, count, temp);
  free(temp);
}

/**
 * Naïve merge sort is best merge sort in C
 * 
 * @param  list   The list to sort from the point that needs sorting
 * @param  count  The number of elements to sort
 * @param  temp   Auxiliary memory
 */
static void _sort_ptr(void** list, size_t count, void** temp)
{
  if (count > 1)
    {
      size_t i = 0, a = count >> 1;
      size_t j = a, b = count - a;
      _sort_ptr(list + 0, a, temp + 0);
      _sort_ptr(list + a, b, temp + a);
      b += a;
      while ((i < a) && (j < b))
	{
          if (*(temp + i) <= *(temp + j))
            *list++ = *(temp + i++);
          else
            *list++ = *(temp + j++);
	}
      while (i < a)
	*list++ = *(temp + i++);
      while (j < b)
	*list++ = *(temp + j++);
      list -= count;
      for (i = 0; i < count; i++)
	*(temp + i) = *(list + i);
    }
  else if (count == 1)
    *temp = *list;
}

/**
 * Naïve merge sort is best merge sort in C
 * 
 * @param  list   The list to sort
 * @param  count  The number of elements to sort
 */
static void sort_ptr(void** list, size_t count)
{
  void** temp = (void**)malloc(count * sizeof(void*));
  _sort_ptr(list, count, temp);
  free(temp);
}


/**
 * Initialises a map
 * 
 * @param  map  The address of the map
 */
static void map_init(args_Map* map)
{
  size_t i;
  void** level;
  map->keys = null;
  map->key_count = 0;
  map->data = level = (void**)malloc(17 * sizeof(void*));
  for (i = 0; i < 17; i++)
    *(level + i) = null;
}

/**
 * Gets the value for a key in a map
 * 
 * @param   map  The address of the map
 * @param   key  The key
 * @return       The value, `null` if not found
 */
static void* map_get(args_Map* map, const char* key)
{
  void** at = map->data;
  while (*key)
    {
      size_t a = (size_t)((*key >> 4) & 15);
      size_t b = (size_t)((*key >> 0) & 15);
      if (*(at + a))
	at = (void**)*(at + a);
      else
	return null;
      if (*(at + b))
	at = (void**)*(at + b);
      else
	return null;
      key++;
    }
  return *(at + 16);
}

/**
 * Sets the value for a key in a map
 * 
 * @param  map    The address of the map
 * @param  key    The key
 * @param  value  The value, `null` to remove, however this does not unlist the key
 */
static void map_put(args_Map* map, const char* key, void* value)
{
  const char* _key = key;
  int new = false;
  void** at = map->data;
  size_t i;
  while (*key)
    {
      size_t a = (size_t)((*key >> 4) & 15);
      size_t b = (size_t)((*key >> 0) & 15);
      if (*(at + a))
	at = (void**)*(at + a);
      else
	{
	  at = (void**)(*(at + a) = (void*)malloc(16 * sizeof(void*)));
	  for (i = 0; i < 16; i++)
	    *(at + i) = null;
	  new = true;
	}
      if (*(at + b))
	at = (void**)*(at + b);
      else
	{
	  at = (void**)(*(at + b) = (void*)malloc(17 * sizeof(void*)));
	  for (i = 0; i < 17; i++)
	    *(at + i) = null;
	  new = true;
	}
      key++;
    }
  *(at + 16) = value;
  if (new)
    {
      map->keys = (const char**)realloc(map->keys, (map->key_count + 1) * sizeof(const char*));
      *(map->keys + map->key_count++) = _key;
    }
}

/**
 * Frees a level and all sublevels in a map
 * 
 * @param  level      The level
 * @param  has_value  Whether the level can hold a value
 */
static void _map_free(void** level, int has_value)
{
  int next_has_value = has_value ^ true;
  size_t i;
  void* value;
  if (level == null)
    return;
  for (i = 0; i < 16; i++)
    _map_free(*(level + i), next_has_value);
  if (has_value)
    if ((value = *(level + 16)))
      {
	if (args_map_values_ptr == args_map_values_size)
	  args_map_values = (void**)realloc(args_map_values, (args_map_values_size <<= 1) * sizeof(void*));
	*(args_map_values + args_map_values_ptr++) = value;
      }
  free(level);
}

/**
 * Frees the resources of a map
 * 
 * @param   map  The address of the map
 * @return       `null`-terminated array of values that you may want to free, but do free this returend array before running this function again
 */
static void** map_free(args_Map* map)
{
  if (map->keys != null)
    free(map->keys);
  map->keys = null;
  
  args_map_values_ptr = 0;
  args_map_values_size = 64;
  args_map_values = (void**)malloc(64 * sizeof(void*));
  _map_free(map->data, true);
  if (args_map_values_ptr == args_map_values_size)
    args_map_values = (void**)realloc(args_map_values, (args_map_values_size + 1) * sizeof(void*));
  *(args_map_values + args_map_values_ptr) = null;
  return args_map_values;
}


/**
 * Dummy trigger
 * 
 * @param  used  The used option alternative
 * @param  std   The standard option alternative
 */
static void noop_2(const char* used, const char* std)
{
  (void) used;
  (void) std;
}

/**
 * Dummy trigger
 * 
 * @param  used   The used option alternative
 * @param  std    The standard option alternative
 * @param  value  The used value
 */
static void noop_3(const char* used, const char* std, char* value)
{
  (void) used;
  (void) std;
  (void) value;
}

/**
 * Default stickless evaluator
 * 
 * @param   The argument
 * @return  Whether the argument can be used without being sticky
 */
static int default_stickless(const char* argument)
{
  return (*argument != '-') && (*argument != '+');
}

