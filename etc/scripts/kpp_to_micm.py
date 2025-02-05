"""
Copyright (C) 2023
National Center for Atmospheric Research,
SPDX-License-Identifier: Apache-2.0

File:
    kpp_to_micm.py

Usage:
    python kpp_to_micm.py
    python kpp_to_micm.py --help

Description:
    kpp_to_micm.py translates KPP config files to MICM JSON config files
    (desginated by the suffixes .kpp, .spc, .eqn, .def)
    from a single directory specified by the
    --kpp_dir and --kpp_name arguments.

    In the initial implementation,
    the KPP sections #ATOMS (not yet used), #DEFFIX,
    #DEFVAR, and #EQUATIONS are read and parsed.
    Equations with the hv reactant are MICM PHOTOLYSIS reactions,
    equations with a single coefficient are assumed to be ARRHENIUS reactions.

TODO:
    (1) Translate stoichiometric coefficients in the equation string
    with more than one digit.
    (2) Add pytest unit test for method micm_equation_json.
    (3+) Add support for many more reaction types ...

Revision History:
    v1.00 2023/08/03 Initial implementation
    v1.01 2023/08/16 Added method parse_kpp_arrhenius
"""

import os
import sys
import argparse
import logging
import json
from glob import glob

__version__ = 'v1.01'


def read_kpp_config(kpp_dir, kpp_name):
    """
    Read all KPP config files in a directory

    Parameters
        (str) kpp_dir: KPP directory

    Returns
        (list of str): all lines from all config files
    """

    suffixes = ['.kpp', '.spc', '.eqn', '.def']

    lines = list()

    for suffix in suffixes:
        files = glob(os.path.join(kpp_dir, kpp_name + '*' + suffix))
        logging.debug(files)
        for filename in files:
            with open(filename, 'r') as f:
                lines.extend(f.readlines())

    # remove empty lines and tabs
    lines = [line.replace('\t', '') for line in lines if line.strip()] 

    for line in lines:
        logging.debug(line.strip())

    return lines


def split_by_section(lines):
    """
    Split KPP config lines by section

    Parameters
        (list of str) lines: all lines config files

    Returns
        (dict of list of str): lines in each section
    """

    sections = {'#ATOMS': [],
                '#DEFVAR': [],
                '#DEFFIX': [],
                '#EQUATIONS': []}

    joined_lines = ''.join(lines)
    section_blocks = joined_lines.split('#')

    for section in sections:
        for section_block in section_blocks:
            if section.replace('#', '') in section_block:
                sections[section].extend(section_block.split('\n')[1:-1])

    return sections


def micm_species_json(lines, fixed=False, tolerance=1.0e-12):
    """
    Generate MICM species JSON

    Parameters
        (list of str) lines: lines of species section
        (bool) fixed: set constant tracer
        (float) tolerance: absolute tolerance

    Returns
        (list of dict): list of MICM species entries
    """

    species_json = list() # list of dict

    for line in lines:
        lhs, rhs = tuple(line.split('='))
        logging.debug((lhs, rhs))
        species_dict = {'name': lhs.strip().lstrip(), 'type': 'CHEM_SPEC'}
        if fixed:
            species_dict['tracer type'] = 'CONSTANT'
        else:
            species_dict['absolute tolerance'] = tolerance
        species_json.append(species_dict)

    return species_json


def parse_kpp_arrhenius(kpp_str):
    """
    Parse KPP Arrhenius reaction

    Parameters
        (str) kpp_str: Arrhenius reaction string

    Returns
        (dict): MICM Arrhenius reaction coefficients


    Arrhenius formula from KPP
    --------------------------

    KPP_REAL ARR_abc( float A0, float B0, float C0 )
    {
      double ARR_RES;

      ARR_RES = (double)A0
        * exp( -(double)B0/TEMP )
        * pow( (TEMP/300.0), (double)C0 );

    return (KPP_REAL)ARR_RES;
    }

    Arrhenius formula from MICM
    ---------------------------

    inline double ArrheniusRateConstant::calculate(
      const double& temperature, const double& pressure) const
    {
    return parameters_.A_ * std::exp(parameters_.C_ / temperature)
      * pow(temperature / parameters_.D_, parameters_.B_) *
      (1.0 + parameters_.E_ * pressure);
    }
    """
    logging.debug(kpp_str)
    coeffs = [float(coeff.replace(' ', '')) for coeff in
        kpp_str.split('(')[1].split(')')[0].split(',')]
    logging.debug(coeffs)
    arr_dict = dict()
    arr_dict['type'] = 'ARRHENIUS' 
    # note the interchange of B and C, and change of sign
    # in the KPP and MICM conventions
    if ('_abc(' in kpp_str):
        arr_dict['A'] = coeffs[0]
        arr_dict['B'] = coeffs[2]
        arr_dict['C'] = - coeffs[1]
        arr_dict['D'] = 300.0
    elif ('_ab(' in kpp_str):
        arr_dict['A'] = coeffs[0]
        arr_dict['C'] = - coeffs[1]
        arr_dict['D'] = 300.0
    elif ('_ac(' in kpp_str):
        arr_dict['A'] = coeffs[0]
        arr_dict['B'] = coeffs[1]
        arr_dict['D'] = 300.0
    else:
        logging.error('unrecognized KPP Arrhenius syntax')
    logging.debug(arr_dict)
    return arr_dict


def micm_equation_json(lines):
    """
    Generate MICM equation JSON

    Parameters
        (list of str) lines: lines of equation section

    Returns
        (list of dict): list of MICM equation entries
    """

    equations = list() # list of dict

    for line in lines:
        logging.debug(line)

        # split on equal sign into left hand and right hand sides 
        lhs, rhs = tuple(line.split('='))

        # extract reaction coefficients
        rhs, coeffs = tuple(rhs.split(':'))
        coeffs = coeffs.replace(';', '')

        # get reactants and products
        reactants = lhs.split('+')
        products = rhs.split('+')

        # extract equation label delimited by < >
        label, reactants[0] = tuple(reactants[0].split('>'))
        label = label.lstrip('<')

        # remove trailing and leading whitespace
        reactants = [reactant.strip().lstrip() for reactant in reactants]
        products = [product.strip().lstrip() for product in products]

        equation_dict = dict()

        if 'SUN' in coeffs:
            equation_dict['type'] = 'PHOTOLYSIS' 
        elif 'ARR' in coeffs:
            equation_dict = parse_kpp_arrhenius(coeffs)
        else:
            # default to Arrhenius with a single coefficient
            coeffs = coeffs.replace('(', '').replace(')', '')
            equation_dict['type'] = 'ARRHENIUS' 
            equation_dict['A'] = float(coeffs)

        equation_dict['reactants'] = dict()
        equation_dict['products'] = dict()

        for reactant in reactants:
            if reactant[0].isdigit():
                equation_dict['reactants'][reactant[1:]] \
                    = {'qty': float(reactant[0])}
            elif 'hv' in reactant:
                pass
            else:
                equation_dict['reactants'][reactant] = dict()

        for product in products:
            if product[0].isdigit():
                equation_dict['products'][product[1:]] \
                    = {'yield': float(product[0])}
            else:
                equation_dict['products'][product] = dict()

        equation_dict['MUSICA name'] = label

        equations.append(equation_dict)

    return equations


if __name__ == '__main__':

    """
    Parse command line arguments
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('--logfile', type=str,
        default=sys.stdout,
        help='log file (default stdout)')
    parser.add_argument('--kpp_dir', type=str,
        default=os.path.join('..', 'configs', 'kpp'),
        help='KPP input config directory')
    parser.add_argument('--kpp_name', type=str,
        default='small_strato',
        help='KPP config name')
    parser.add_argument('--micm_dir', type=str,
        default=os.path.join('..', 'configs', 'micm'),
        help='MICM output species config file')
    parser.add_argument('--mechanism', type=str,
        default='Chapman',
        help='mechanism name')
    parser.add_argument('--debug', action='store_true',
        help='set logging level to debug')
    args = parser.parse_args()

    """
    Setup logging
    """
    logging_level = logging.DEBUG if args.debug else logging.INFO
    logging.basicConfig(stream=args.logfile, level=logging_level)

    """
    Read KPP config files
    """
    lines = read_kpp_config(args.kpp_dir, args.kpp_name)

    """
    Split KPP config by section
    """
    sections = split_by_section(lines)
    for section in sections:
        logging.info('____ KPP section %s ____' % section)
        for line in sections[section]:
            logging.info(line)
        print('\n')

    """
    Generate MICM species JSON from KPP #DEFFIX section
    """
    deffix_json = micm_species_json(sections['#DEFFIX'], fixed=True)

    """
    Generate MICM species JSON from KPP #DEFVAR section
    """
    defvar_json = micm_species_json(sections['#DEFVAR'])

    """
    Generate MICM equations JSON from KPP #EQUATIONS section
    """
    equations_json = micm_equation_json(sections['#EQUATIONS'])

    """
    Assemble MICM species JSON
    """
    micm_species_json = {'camp-data': deffix_json + defvar_json}
    micm_species_json_str = json.dumps(micm_species_json, indent=4)
    logging.info('____ MICM species ____')
    logging.info(micm_species_json_str)
    print('\n')

    """
    Assemble MICM reactions JSON
    """
    micm_reactions_json = {'camp-data':
        [{'name': args.mechanism, 'type': 'MECHANISM', 'reactions': equations_json}]}
    micm_reactions_json_str = json.dumps(micm_reactions_json, indent=4)
    logging.info('____ MICM reactions ____')
    logging.info(micm_reactions_json_str)
    print('\n')

    """
    Write MICM JSON
    """
    micm_mechanism_dir = os.path.join(args.micm_dir, args.mechanism)
    if not os.path.exists(args.micm_dir):
        os.mkdir(args.micm_dir)
    if not os.path.exists(micm_mechanism_dir):
        os.mkdir(micm_mechanism_dir)
    with open(os.path.join(micm_mechanism_dir, 'species.json'), 'w') as f:
        json.dump(micm_species_json, f, indent=4)
    with open(os.path.join(micm_mechanism_dir, 'reactions.json'), 'w') as f:
        json.dump(micm_reactions_json, f, indent=4)

