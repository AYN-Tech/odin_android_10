#!/usr/bin/env python
#
# Copyright 2019, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import os
import sys
from resource_utils import get_all_resources, get_resources_from_single_file, remove_layout_resources
from git_utils import has_chassis_changes

# path to 'packages/apps/Car/libs/car-ui-lib/'
ROOT_FOLDER = os.path.dirname(os.path.abspath(__file__)) + '/../..'
OUTPUT_FILE_PATH = ROOT_FOLDER + '/tests/apitest/'

"""
Script used to update the 'current.xml' file. This is being used as part of pre-submits to
verify whether resources previously exposed to OEMs are being changed by a CL, potentially
breaking existing customizations.

Example usage: python auto-generate-resources.py current.xml
"""
def main():
    parser = argparse.ArgumentParser(description='Check if any existing resources are modified.')
    parser.add_argument('--sha', help='Git hash of current changes. This script will not run if this is provided and there are no chassis changes.')
    parser.add_argument('-f', '--file', default='current.xml', help='Name of output file.')
    parser.add_argument('-c', '--compare', action='store_true',
                        help='Pass this flag if resources need to be compared.')
    args = parser.parse_args()

    if not has_chassis_changes(args.sha):
        # Don't run because there were no chassis changes
        return

    output_file = args.file or 'current.xml'
    if args.compare:
        compare_resources(ROOT_FOLDER+'/res', OUTPUT_FILE_PATH + 'current.xml')
    else:
        generate_current_file(ROOT_FOLDER+'/res', output_file)

def generate_current_file(res_folder, output_file='current.xml'):
    resources = remove_layout_resources(get_all_resources(res_folder))
    resources = sorted(resources, key=lambda x: x.type + x.name)

    # defer importing lxml to here so that people who aren't editing chassis don't have to have
    # lxml installed
    import lxml.etree as etree

    root = etree.Element('resources')

    root.addprevious(etree.Comment('This file is AUTO GENERATED, DO NOT EDIT MANUALLY.'))
    for resource in resources:
        item = etree.SubElement(root, 'public')
        item.set('type', resource.type)
        item.set('name', resource.name)

    data = etree.ElementTree(root)

    with open(OUTPUT_FILE_PATH + output_file, 'w') as f:
        data.write(f, pretty_print=True, xml_declaration=True, encoding='utf-8')

def compare_resources(res_folder, res_public_file):
    old_mapping = get_resources_from_single_file(res_public_file)

    new_mapping = remove_layout_resources(get_all_resources(res_folder))

    removed = old_mapping.difference(new_mapping)
    added = new_mapping.difference(old_mapping)
    if len(removed) > 0:
        print('Resources removed:\n' + '\n'.join(map(lambda x: str(x), removed)))
    if len(added) > 0:
        print('Resources added:\n' + '\n'.join(map(lambda x: str(x), added)))

    if len(added) + len(removed) > 0:
        print("Some resource have been modified. If this is intentional please " +
              "run 'python auto-generate-resources.py' again and submit the new current.xml")
        sys.exit(1)

if __name__ == '__main__':
    main()
