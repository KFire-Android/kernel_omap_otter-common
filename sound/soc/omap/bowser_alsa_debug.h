#ifndef __BOWSER_ALSA_DEBUG_H__
#define __BOWSER_ALSA_DEBUG_H__


#undef PDEBUG
#ifdef ALSA_DEBUG
#	define PDEBUG(fmt, args...) printk(KERN_INFO "ALSA: " fmt, ## args)
#else
#	define PDEBUG(fmt, args...)
#endif // PDEBUG

#undef PDEBUGG
#define PDEBUGG(fmt, args...)  //nothing: it's a place holder

#endif  //__BOWSER_ALSA_DEBUG_H__
