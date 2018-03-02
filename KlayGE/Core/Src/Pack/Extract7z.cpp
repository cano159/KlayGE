/**
 * @file Extract7z.cpp
 * @author Minmin Gong
 *
 * @section DESCRIPTION
 *
 * This source file is part of KlayGE
 * For the latest info, see http://www.klayge.org
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * You may alternatively use this source under the terms of
 * the KlayGE Proprietary License (KPL). You can obtained such a license
 * from http://www.klayge.org/licensing/.
 */

#include <KlayGE/KlayGE.hpp>
#define INITGUID
#include <KFL/Util.hpp>
#include <KFL/ErrorHandling.hpp>
#include <KFL/COMPtr.hpp>
#include <KlayGE/Package.hpp>

#include <boost/assert.hpp>

#include <KlayGE/Extract7z.hpp>

namespace KlayGE
{
	uint32_t Find7z(ResIdentifierPtr const & archive_is,
								std::string_view password,
								std::string_view extract_file_path)
	{
		Package package(archive_is, password);
		return package.Find(extract_file_path);
	}

	void Extract7z(ResIdentifierPtr const & archive_is,
		std::string_view password,
		std::string_view extract_file_path,
		std::shared_ptr<std::ostream> const & os)
	{
		Package package(archive_is, password);
		return package.Extract(extract_file_path, os);
	}
}
