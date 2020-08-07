/*
 *  Copyright (C) 2020 Team Kodi <https://kodi.tv>
 *  Copyright (c) 2011-2020 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

/*
 * Code taken SACD Decoder plugin (from foo_input_sacd) by Team Kodi.
 * Original author Maxim V.Anisiutkin.
 */

#include "sacd_core.h"
#include "../Settings.h"

#include <memory>

std::string getFileExt(const std::string& s)
{

  size_t i = s.rfind('.', s.length());
  if (i != std::string::npos)
  {
    return (s.substr(i + 1, s.length() - i));
  }

  return ("");
}

void replaceExt(std::string& s, const std::string& newExt)
{

  std::string::size_type i = s.rfind('.', s.length());

  if (i != std::string::npos)
  {
    s.replace(i + 1, newExt.length(), newExt);
  }
}

bool icasecmp(const std::string& l, const std::string& r)
{
  return l.size() == r.size() && equal(l.cbegin(), l.cend(), r.cbegin(),
                                       [](std::string::value_type l1, std::string::value_type r1) {
                                         return toupper(l1) == toupper(r1);
                                       });
}


bool sacd_core_t::g_is_our_content_type(const std::string& p_type)
{
  return false;
}

bool sacd_core_t::g_is_our_path(const std::string& path, const std::string& ext)
{
  if (icasecmp(ext, "WV") && !icasecmp(path, "!.WV") && g_dsd_in_wavpack(path.c_str()))
  {
    return true;
  }
  std::string filename_ext = kodi::vfs::GetFileName(path);
  return ((icasecmp(ext, "ISO") || icasecmp(ext, "DAT")) && sacd_disc_t::g_is_sacd(path.c_str())) ||
         icasecmp(ext, "DFF") || icasecmp(ext, "DSF"); /* ||
         (icasecmp(filename_ext, "") || icasecmp(filename_ext, "MASTER1.TOC")) &&
             path.length() > 7 && sacd_disc_t::g_is_sacd(path[7]);*/
}

sacd_core_t::sacd_core_t() : media_type(media_type_e::INVALID), access_mode(ACCESS_MODE_NULL)
{
}

bool sacd_core_t::open(const std::string& path)
{
  std::string filename_ext = kodi::vfs::GetFileName(path);
  std::string ext = getFileExt(filename_ext);
  auto is_sacd_disc = false;
  media_type = media_type_e::INVALID;
  if (icasecmp(ext, "ISO") == 0)
  {
    media_type = media_type_e::ISO;
  }
  else if (icasecmp(ext, "DAT") == 0)
  {
    media_type = media_type_e::ISO;
  }
  else if (icasecmp(ext, "DFF") == 0)
  {
    media_type = media_type_e::DSDIFF;
  }
  else if (icasecmp(ext, "DSF") == 0)
  {
    media_type = media_type_e::DSF;
  }
  else if (icasecmp(ext, "WV") == 0)
  {
    media_type = media_type_e::WAVPACK;
  }
  else if ((icasecmp(filename_ext, "") == 0 || icasecmp(filename_ext, "MASTER1.TOC") == 0) &&
           path.length() > 7 && sacd_disc_t::g_is_sacd(path[7]))
  {
    media_type = media_type_e::ISO;
    is_sacd_disc = true;
  }
  if (media_type == media_type_e::INVALID)
  {
    kodi::Log(ADDON_LOG_ERROR, "unsupported format '%s'\n", path.c_str());
    return false;
  }
  if (is_sacd_disc)
  {
    sacd_media = std::make_unique<sacd_media_disc_t>();
    if (!sacd_media)
    {
      kodi::Log(ADDON_LOG_ERROR, "memory overflow '%s'\n", path.c_str());
      return false;
    }
  }
  else
  {
    sacd_media = std::make_unique<sacd_media_file_t>();
    if (!sacd_media)
    {
      kodi::Log(ADDON_LOG_ERROR, "memory overflow '%s'\n", path.c_str());
      return false;
    }
  }
  switch (media_type)
  {
    case media_type_e::ISO:
      sacd_reader = std::make_unique<sacd_disc_t>();
      if (!sacd_reader)
      {
        kodi::Log(ADDON_LOG_ERROR, "memory overflow '%s'\n", path.c_str());
        return false;
      }
      break;
    case media_type_e::DSDIFF:
      sacd_reader = std::make_unique<sacd_dsdiff_t>();
      if (!sacd_reader)
      {
        kodi::Log(ADDON_LOG_ERROR, "memory overflow '%s'\n", path.c_str());
        return false;
      }
      break;
    case media_type_e::DSF:
      sacd_reader = std::make_unique<sacd_dsf_t>();
      if (!sacd_reader)
      {
        kodi::Log(ADDON_LOG_ERROR, "memory overflow '%s'\n", path.c_str());
        return false;
      }
      break;
    case media_type_e::WAVPACK:
      sacd_reader = std::make_unique<sacd_wavpack_t>();
      if (!sacd_reader)
      {
        kodi::Log(ADDON_LOG_ERROR, "memory overflow '%s'\n", path.c_str());
        return false;
      }
      break;
    default:
      kodi::Log(ADDON_LOG_ERROR, "unsupported format %i on '%s'\n", media_type, path.c_str());
      return false;
  }
  access_mode = ACCESS_MODE_NULL;
  switch (CSACDSettings::GetInstance().GetSpeakerArea())
  {
    case 0:
      access_mode |= ACCESS_MODE_TWOCH | ACCESS_MODE_MULCH;
      break;
    case 1:
      access_mode |= ACCESS_MODE_TWOCH;
      break;
    case 2:
      access_mode |= ACCESS_MODE_MULCH;
      break;
  }
  if (CSACDSettings::GetInstance().GetFullPlayback())
  {
    access_mode |= ACCESS_MODE_FULL_PLAYBACK;
  }

  if (!sacd_media->open(path, false))
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to open media type %i on '%s'\n", media_type, path.c_str());
    return false;
  }

  if (!sacd_reader->open(sacd_media.get()))
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to open media reader for type %i on '%s'\n", media_type,
              path.c_str());
    return false;
  }

  sacd_reader->set_mode(access_mode);

  return true;
}
