#!/usr/bin/env python3
"""Generate Meridian Hardware & Home — complete relational store database.

Output: data/meridian_hardware.json (~10-15 MB)
Tables: store, zones, departments, suppliers, employees, products, customers,
        accounts, shifts, sales, sale_items, inventory_snapshots, purchase_orders,
        expenses, daily_summaries
"""
import json, random, math, os, hashlib
from datetime import date, timedelta
from collections import defaultdict

# ════════════════════════════════════════════════════════════════
# Config
# ════════════════════════════════════════════════════════════════
SEED = 42
random.seed(SEED)
START = date(2024, 7, 1)
END = date(2026, 3, 22)
TAX_RATE = 0.0925
BASE_DAILY_SALES = 16
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "meridian_hardware.json")

# ════════════════════════════════════════════════════════════════
# Helpers
# ════════════════════════════════════════════════════════════════
def fmt(d): return d.isoformat()
def money(x): return round(x, 2)
def pct(x): return round(x * 100, 1)

def seasonal_mult(d):
    """Month-based seasonality multiplier."""
    return {1:0.78, 2:0.82, 3:1.25, 4:1.35, 5:1.30, 6:1.22,
            7:1.18, 8:1.15, 9:1.00, 10:0.95, 11:1.05, 12:1.42}[d.month]

def dow_mult(d):
    """Day-of-week multiplier. 0=Mon, 6=Sun."""
    return [0.70, 0.78, 0.85, 0.95, 1.22, 1.55, 1.35][d.weekday()]

def growth_mult(d):
    """Year-over-year growth factor (~7% annual)."""
    months_from_start = (d.year - START.year) * 12 + (d.month - START.month)
    return 1.0 + 0.07 * (months_from_start / 12.0)

def dept_seasonal_boost(dept_id, month):
    """Department-specific seasonal adjustment."""
    boosts = {
        1: {3:1.1, 4:1.2, 5:1.15, 6:1.1},          # Lumber: spring building
        6: {3:1.3, 4:1.4, 5:1.35, 6:1.2, 7:1.1},    # Garden: spring/summer
        8: {10:1.2, 11:1.4, 12:1.8},                  # Seasonal: holiday
        5: {4:1.2, 5:1.3, 6:1.15, 9:1.1},             # Paint: spring/fall
    }
    return boosts.get(dept_id, {}).get(month, 1.0)

def is_black_friday(d):
    """Return True if d is Black Friday (4th Thursday of November)."""
    if d.month != 11: return False
    # Find 4th Thursday
    first = date(d.year, 11, 1)
    # Day of week: 0=Mon..6=Sun; Thursday=3
    offset = (3 - first.weekday()) % 7
    fourth_thu = first + timedelta(days=offset + 21)
    return d == fourth_thu or d == fourth_thu + timedelta(days=1)

def rand_time(h_low, h_high):
    """Random HH:MM between hour bounds."""
    h = random.randint(h_low, h_high - 1)
    m = random.choice([0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55])
    return f"{h:02d}:{m:02d}"

def gen_email(first, last, domain="meridianhw.com"):
    return f"{first[0].lower()}{last.lower()}@{domain}"

def gen_customer_email(first, last):
    domains = ["gmail.com","yahoo.com","outlook.com","icloud.com","hotmail.com","aol.com","proton.me"]
    tag = random.randint(1, 999)
    return f"{first.lower()}.{last.lower()}{tag}@{random.choice(domains)}"

# ════════════════════════════════════════════════════════════════
# 1. Store
# ════════════════════════════════════════════════════════════════
store = {
    "id": 1,
    "name": "Meridian Hardware & Home",
    "address": "4200 Meridian Ave, San Jose, CA 95124",
    "phone": "(408) 555-0142",
    "email": "info@meridianhardware.com",
    "website": "www.meridianhardware.com",
    "openDate": "2020-03-15",
    "sqft": 28000,
    "operatingHours": {
        "weekday": {"open": "07:00", "close": "21:00"},
        "weekend": {"open": "08:00", "close": "20:00"}
    },
    "taxRate": TAX_RATE
}

# ════════════════════════════════════════════════════════════════
# 2. Zones — physical floor layout
# ════════════════════════════════════════════════════════════════
zones = [
    {"id": "A", "name": "Front (Registers & Service)", "sqft": 3000,
     "position": {"x": 0.0, "y": 0.0, "w": 1.0, "h": 0.15}},
    {"id": "B", "name": "Left Wing (Lumber & Building)", "sqft": 6000,
     "position": {"x": 0.0, "y": 0.15, "w": 0.35, "h": 0.85}},
    {"id": "C", "name": "Center Aisles (Tools/Plumbing/Electrical)", "sqft": 7000,
     "position": {"x": 0.35, "y": 0.15, "w": 0.30, "h": 0.55}},
    {"id": "D", "name": "Right Wing (Paint & Décor)", "sqft": 5000,
     "position": {"x": 0.65, "y": 0.15, "w": 0.35, "h": 0.55}},
    {"id": "E", "name": "Garden Center (Outdoor)", "sqft": 4500,
     "position": {"x": 0.35, "y": 0.70, "w": 0.65, "h": 0.30}},
    {"id": "F", "name": "Warehouse & Stockroom", "sqft": 2500,
     "position": {"x": 0.0, "y": 0.0, "w": 0.0, "h": 0.0}},
]

# ════════════════════════════════════════════════════════════════
# 3. Departments
# ════════════════════════════════════════════════════════════════
departments = [
    {"id": 1, "name": "Lumber & Building Materials", "floorZoneId": "B", "managerId": None},
    {"id": 2, "name": "Tools & Hardware",            "floorZoneId": "C", "managerId": None},
    {"id": 3, "name": "Plumbing",                    "floorZoneId": "C", "managerId": None},
    {"id": 4, "name": "Electrical",                  "floorZoneId": "C", "managerId": None},
    {"id": 5, "name": "Paint & Coatings",            "floorZoneId": "D", "managerId": None},
    {"id": 6, "name": "Garden & Outdoor",            "floorZoneId": "E", "managerId": None},
    {"id": 7, "name": "Home Décor",                  "floorZoneId": "D", "managerId": None},
    {"id": 8, "name": "Seasonal & Holiday",          "floorZoneId": "E", "managerId": None},
]

# ════════════════════════════════════════════════════════════════
# 4. Suppliers
# ════════════════════════════════════════════════════════════════
suppliers = [
    {"id":1,  "name":"Pacific Lumber Co",        "region":"West",    "leadTimeDays":7,  "contactEmail":"orders@paclumber.com",      "phone":"(503) 555-0188"},
    {"id":2,  "name":"Midwest Fastener Supply",  "region":"Midwest", "leadTimeDays":5,  "contactEmail":"sales@midwestfast.com",     "phone":"(312) 555-0234"},
    {"id":3,  "name":"National Tool Distributors","region":"Southeast","leadTimeDays":4, "contactEmail":"orders@natltool.com",       "phone":"(704) 555-0371"},
    {"id":4,  "name":"ProPlumb Supply",          "region":"South",   "leadTimeDays":5,  "contactEmail":"wholesale@proplumb.com",    "phone":"(214) 555-0492"},
    {"id":5,  "name":"ElectroPro Wholesale",     "region":"Southeast","leadTimeDays":4, "contactEmail":"bulk@electropro.com",       "phone":"(404) 555-0518"},
    {"id":6,  "name":"ColorMaster Paints",       "region":"Midwest", "leadTimeDays":6,  "contactEmail":"orders@colormaster.com",    "phone":"(513) 555-0627"},
    {"id":7,  "name":"GreenGrow Distributors",   "region":"West",    "leadTimeDays":3,  "contactEmail":"supply@greengrow.com",      "phone":"(916) 555-0745"},
    {"id":8,  "name":"HomeStyle Imports",        "region":"West",    "leadTimeDays":10, "contactEmail":"orders@homestyle-imp.com",  "phone":"(213) 555-0853"},
    {"id":9,  "name":"SafeGuard Products",       "region":"Northeast","leadTimeDays":6, "contactEmail":"sales@safeguardpro.com",    "phone":"(617) 555-0961"},
    {"id":10, "name":"BuildRight Materials",     "region":"Southwest","leadTimeDays":8, "contactEmail":"orders@buildright.com",     "phone":"(602) 555-1078"},
    {"id":11, "name":"Apex Tool Group",          "region":"Mid-Atlantic","leadTimeDays":5,"contactEmail":"dist@apextool.com",       "phone":"(410) 555-1182"},
    {"id":12, "name":"SeasonAll Supply",         "region":"Mountain", "leadTimeDays":7, "contactEmail":"orders@seasonall.com",      "phone":"(303) 555-1294"},
]

# dept → primary supplier IDs
DEPT_SUPPLIERS = {1:[1,10], 2:[2,3,11], 3:[4], 4:[5], 5:[6], 6:[7], 7:[8], 8:[9,12]}

# ════════════════════════════════════════════════════════════════
# 5. Employees (35)
# ════════════════════════════════════════════════════════════════
_emp_raw = [
    # (id, first, last, role, deptId, hireDate, hourlyRate, status, termDate)
    (1,  "Patricia","Chen",       "Store Manager",      None, "2020-03-15", 38.50, "active", None),
    (2,  "Marcus",  "Washington", "Assistant Manager",   None, "2020-04-01", 28.00, "active", None),
    (3,  "Sarah",   "Johansson",  "Assistant Manager",   None, "2021-01-10", 27.50, "active", None),
    (4,  "Diego",   "Ramirez",    "Department Lead",     1,   "2020-05-01", 24.00, "active", None),
    (5,  "James",   "O'Brien",    "Department Lead",     2,   "2020-03-20", 24.50, "active", None),
    (6,  "Aisha",   "Patel",      "Department Lead",     3,   "2020-06-15", 23.50, "active", None),
    (7,  "Tom",     "Kowalski",   "Department Lead",     4,   "2020-07-01", 23.50, "active", None),
    (8,  "Linda",   "Nguyen",     "Department Lead",     5,   "2021-03-01", 23.00, "active", None),
    (9,  "Robert",  "Garcia",     "Department Lead",     6,   "2020-04-15", 24.00, "active", None),
    (10, "Kenji",   "Tanaka",     "Department Lead",     7,   "2022-01-10", 22.50, "active", None),
    (11, "Fatima",  "Al-Hassan",  "Department Lead",     8,   "2021-08-01", 22.50, "active", None),
    (12, "Mike",    "Sullivan",   "Sales Associate",     1,   "2022-03-15", 18.50, "active", None),
    (13, "Crystal", "Johnson",    "Sales Associate",     1,   "2023-06-01", 17.50, "active", None),
    (14, "DeShawn", "Williams",   "Sales Associate",     2,   "2021-09-20", 19.00, "active", None),
    (15, "Emily",   "Larson",     "Sales Associate",     2,   "2023-01-15", 17.50, "active", None),
    (16, "Raj",     "Mehta",      "Sales Associate",     3,   "2022-06-01", 18.00, "active", None),
    (17, "Olga",    "Petrov",     "Sales Associate",     4,   "2022-08-15", 18.00, "active", None),
    (18, "Carlos",  "Mendoza",    "Sales Associate",     5,   "2023-03-01", 17.50, "active", None),
    (19, "Beth",    "Reynolds",   "Sales Associate",     6,   "2021-04-10", 18.50, "active", None),
    (20, "Tyler",   "Kim",        "Sales Associate",     6,   "2023-07-01", 17.00, "active", None),
    (21, "Jasmine", "Brown",      "Sales Associate",     7,   "2022-11-01", 17.50, "active", None),
    (22, "Pavel",   "Novak",      "Sales Associate",     8,   "2023-09-15", 17.00, "active", None),
    (23, "Hannah",  "Schmidt",    "Sales Associate",     2,   "2024-01-08", 17.00, "active", None),
    (24, "Andre",   "Lewis",      "Sales Associate",     5,   "2024-06-01", 17.00, "active", None),
    (25, "Maria",   "Santos",     "Sales Associate",     6,   "2024-09-01", 17.00, "active", None),
    (26, "Sam",     "O'Donnell",  "Sales Associate",     3,   "2023-04-10", 17.50, "terminated", "2025-08-15"),
    (27, "Courtney","Adams",      "Cashier",             None, "2021-06-01", 16.50, "active", None),
    (28, "Darius",  "Jackson",    "Cashier",             None, "2022-02-14", 16.50, "active", None),
    (29, "Mei Lin", "Wu",         "Cashier",             None, "2023-05-01", 16.00, "active", None),
    (30, "Terri",   "Blake",      "Cashier",             None, "2024-03-15", 16.00, "active", None),
    (31, "Jake",    "Morrison",   "Stock Clerk",         None, "2020-08-01", 17.00, "active", None),
    (32, "Eduardo", "Flores",     "Stock Clerk",         None, "2022-04-10", 16.50, "active", None),
    (33, "Nina",    "Voronova",   "Stock Clerk",         None, "2024-01-22", 16.00, "active", None),
    (34, "Angela",  "Martinez",   "Customer Service",    None, "2021-02-01", 18.00, "active", None),
    (35, "Ryan",    "Cooper",     "Customer Service",    None, "2023-08-01", 17.50, "active", None),
]

employees = []
for e in _emp_raw:
    emp = {
        "id": e[0], "firstName": e[1], "lastName": e[2], "role": e[3],
        "departmentId": e[4], "hireDate": e[5], "hourlyRate": e[6],
        "status": e[7], "terminationDate": e[8],
        "email": gen_email(e[1], e[2]),
    }
    employees.append(emp)

# Set department managers
for dept in departments:
    for emp in employees:
        if emp["role"] == "Department Lead" and emp["departmentId"] == dept["id"]:
            dept["managerId"] = emp["id"]
            break

# ════════════════════════════════════════════════════════════════
# 6. Products (150)
# ════════════════════════════════════════════════════════════════
# (name, category, unitCost, unitPrice, weightLbs, popularity 1-100)
_PRODS = {
    1: [  # Lumber & Building Materials (20)
        ("2x4x8 Kiln-Dried Stud",        "framing",      3.48,  5.49,  9.0,  98),
        ("2x6x8 #2 Lumber",              "framing",      5.98,  9.49,  13.0, 72),
        ("4x8 Plywood Sheet 1/2in",      "sheathing",    28.50, 42.99, 45.0, 65),
        ("4x8 OSB Sheathing 7/16in",     "sheathing",    18.90, 29.49, 42.0, 55),
        ("2x4x12 Pressure-Treated",      "treated",      7.20,  11.49, 14.0, 48),
        ("1x6x8 Cedar Fence Board",      "fencing",      4.80,  7.99,  6.0,  52),
        ("Concrete Mix 80lb Bag",        "concrete",     4.50,  6.99,  80.0, 85),
        ("Drywall Sheet 4x8 1/2in",     "drywall",      8.90,  13.99, 52.0, 60),
        ("Joint Compound 5-Gallon",      "drywall",      10.50, 17.49, 62.0, 45),
        ("Drywall Tape 300ft Roll",      "drywall",      3.20,  5.99,  1.0,  40),
        ("R-13 Insulation Batt 15in",    "insulation",   0.65,  1.09,  1.5,  35),
        ("House Wrap Roll 9x150",        "weatherproof", 98.00, 159.99,18.0, 20),
        ("PVC Trim Board 1x4x8",        "trim",         8.50,  14.99, 4.0,  30),
        ("Deck Screws #8 5lb Box",       "fasteners",    18.00, 29.99, 5.0,  70),
        ("Framing Nails 16d 5lb Box",    "fasteners",    12.00, 19.99, 5.0,  65),
        ("Construction Adhesive 10oz",   "adhesives",    3.50,  5.99,  0.8,  55),
        ("Flashing Tape 4in x 75ft",    "weatherproof", 15.00, 24.99, 2.0,  25),
        ("Rebar #4 x 10ft",             "concrete",     5.80,  9.49,  13.0, 22),
        ("Play Sand 50lb Bag",          "aggregate",    3.80,  5.49,  50.0, 42),
        ("Pea Gravel 50lb Bag",         "aggregate",    4.20,  6.49,  50.0, 38),
    ],
    2: [  # Tools & Hardware (25)
        ("Claw Hammer 16oz",             "hand tools",   8.50,  14.99, 1.5,  90),
        ("Tape Measure 25ft",            "measuring",    6.50,  12.99, 0.5,  95),
        ("Cordless Drill/Driver 20V Kit","power tools",  89.00, 149.99,5.0,  75),
        ("Circular Saw 7-1/4in 15A",    "power tools",  65.00, 109.99,10.0, 45),
        ("Level 48in Aluminum",          "measuring",    18.00, 32.99, 3.0,  50),
        ("Screwdriver Set 10-Piece",     "hand tools",   9.00,  16.99, 2.0,  85),
        ("Adjustable Wrench 10in",       "hand tools",   7.50,  13.99, 1.5,  60),
        ("Pliers Set 3-Piece",           "hand tools",   12.00, 21.99, 2.0,  65),
        ("Socket Set 40-Piece 1/4+3/8",  "hand tools",  22.00, 39.99, 5.0,  55),
        ("Utility Knife Retractable",    "hand tools",   4.00,  7.99,  0.3,  88),
        ("Allen Key Set Metric+SAE",     "hand tools",   5.50,  9.99,  0.5,  50),
        ("Drill Bit Set 29-Piece HSS",  "accessories",  15.00, 27.99, 2.0,  70),
        ("Jigsaw Cordless 20V",          "power tools",  75.00, 129.99,6.0,  30),
        ("Random Orbital Sander 5in",    "power tools",  42.00, 69.99, 4.0,  35),
        ("Router Combo Kit 1/4+1/2in",  "power tools",  110.00,189.99,12.0, 18),
        ("Sawhorse Pair Folding",        "workholding",  28.00, 49.99, 15.0, 40),
        ("Clamp Set 6-Piece Bar",        "workholding",  20.00, 34.99, 8.0,  42),
        ("Safety Glasses Clear 3-Pack", "safety",       4.00,  7.99,  0.3,  78),
        ("Work Gloves Leather Pair",     "safety",       7.00,  12.99, 0.5,  72),
        ("Tool Belt 11-Pocket",          "storage",      15.00, 27.99, 1.5,  38),
        ("Stud Finder Electronic",       "measuring",    14.00, 24.99, 0.5,  48),
        ("Chalk Line Reel Kit",          "measuring",    5.50,  9.99,  0.8,  35),
        ("Speed Square 7in",            "measuring",    4.50,  8.99,  0.5,  55),
        ("Pry Bar 18in Flat",           "hand tools",   8.00,  14.99, 2.0,  32),
        ("Wire Stripper/Crimper",        "hand tools",   6.50,  11.99, 0.5,  45),
    ],
    3: [  # Plumbing (18)
        ("PVC Pipe 1-1/2in x 10ft",     "pipe",         3.20,  5.49,  3.0,  80),
        ("PVC Elbow 1-1/2in 90°",       "fittings",     0.65,  1.29,  0.1,  85),
        ("Copper Pipe 3/4in x 10ft",    "pipe",         18.50, 29.99, 3.5,  42),
        ("SharkBite Coupling 3/4in",    "fittings",     5.80,  9.99,  0.1,  55),
        ("Toilet Repair Kit Universal", "repair kits",  8.50,  14.99, 2.0,  72),
        ("Wax Ring Toilet w/Flange",    "repair kits",  2.50,  4.99,  1.0,  60),
        ("Kitchen Faucet Single-Handle","fixtures",     55.00, 89.99, 6.0,  35),
        ("Bathroom Faucet Chrome 4in", "fixtures",     32.00, 54.99, 4.0,  38),
        ("Supply Line Braided 3/8x20", "supply lines", 4.50,  8.49,  0.3,  65),
        ("P-Trap 1-1/2in PVC",         "fittings",     2.80,  4.99,  0.3,  70),
        ("Pipe Wrench 14in",            "tools",        12.00, 21.99, 3.0,  40),
        ("Teflon Tape 1/2in Roll",      "sealants",     0.60,  1.49,  0.1,  92),
        ("PVC Cement 8oz",              "sealants",     3.50,  6.49,  0.5,  75),
        ("Shutoff Valve 1/2in",         "valves",       5.50,  9.99,  0.3,  58),
        ("Drain Snake 25ft",            "tools",        12.00, 22.99, 2.0,  45),
        ("Garbage Disposal 1/2 HP",     "fixtures",     65.00, 109.99,12.0, 15),
        ("Water Heater Element 4500W", "parts",        9.00,  16.99, 1.0,  20),
        ("Hose Bibb Frost-Free 12in",  "outdoor",      12.00, 21.99, 2.0,  30),
    ],
    4: [  # Electrical (18)
        ("Romex 14/2 NM-B 250ft",       "wire",         68.00, 109.99,18.0, 55),
        ("Outlet Receptacle 15A White", "devices",      0.45,  0.99,  0.1,  90),
        ("GFCI Outlet 20A White",       "devices",      9.50,  17.99, 0.2,  65),
        ("Light Switch Single-Pole",    "devices",      0.40,  0.89,  0.1,  88),
        ("Wire Nuts Assorted 100-Pack","connectors",   3.50,  6.99,  0.5,  82),
        ("Electrical Box Single-Gang", "boxes",        0.35,  0.79,  0.1,  85),
        ("Circuit Breaker 20A SP",      "breakers",     5.50,  9.99,  0.3,  50),
        ("LED Bulb 60W-Eq 4-Pack",     "lighting",     3.50,  7.99,  0.5,  95),
        ("Recessed Light 6in LED",      "lighting",     8.50,  15.99, 1.0,  58),
        ("Ceiling Fan 52in w/Light",    "fans",         85.00, 149.99,18.0, 22),
        ("Extension Cord 50ft 16/3",   "cords",        12.00, 21.99, 3.0,  68),
        ("Surge Protector 6-Outlet",    "protection",   8.00,  14.99, 1.0,  55),
        ("Wire 12-Gauge THHN 100ft",   "wire",         22.00, 37.99, 4.0,  45),
        ("Conduit EMT 3/4in x 10ft",   "conduit",      3.80,  6.99,  2.5,  35),
        ("Voltage Tester Non-Contact", "tools",        8.00,  14.99, 0.2,  60),
        ("Junction Box 4in Square",    "boxes",        1.20,  2.49,  0.3,  42),
        ("Dimmer Switch LED-Compat",   "devices",      12.00, 22.99, 0.2,  48),
        ("Motion Sensor Floodlight",   "lighting",     22.00, 39.99, 4.0,  35),
    ],
    5: [  # Paint & Coatings (18)
        ("Interior Latex Flat Gal White",    "interior paint", 14.00, 32.99, 12.0, 92),
        ("Interior Latex Eggshell Gal Tint", "interior paint", 16.00, 36.99, 12.0, 80),
        ("Exterior Acrylic Satin Gallon",    "exterior paint", 20.00, 39.99, 12.0, 55),
        ("Primer Gallon Multi-Surface",      "primer",         12.00, 24.99, 12.0, 68),
        ("Paint Roller Cover 9in 3-Pack",    "applicators",    4.50,  8.99,  0.5,  88),
        ("Paint Brush 2in Angled",           "applicators",    3.00,  6.99,  0.2,  85),
        ("Painter's Tape 1.88in x 60yd",   "prep",           3.80,  7.49,  0.3,  90),
        ("Drop Cloth Canvas 9x12",           "prep",           8.00,  14.99, 3.0,  65),
        ("Paint Tray Liner 3-Pack",          "applicators",    2.00,  3.99,  0.5,  72),
        ("Spray Paint Matte Black 12oz",     "spray paint",    3.50,  5.99,  1.0,  60),
        ("Wood Stain Interior Quart",        "stain",          7.50,  13.99, 2.5,  50),
        ("Polyurethane Clear Satin Qt",      "finish",         8.00,  14.99, 2.5,  48),
        ("Silicone Caulk Clear 10.1oz",      "caulk",          3.50,  6.49,  0.8,  75),
        ("Caulk Gun Standard",               "tools",          3.00,  5.99,  0.5,  55),
        ("Sandpaper Assorted 20-Sheet",      "prep",           4.00,  7.99,  0.5,  62),
        ("Paint Sprayer HVLP Electric",      "sprayers",       55.00, 89.99, 6.0,  15),
        ("Deck Stain Semi-Trans Gallon",     "stain",          20.00, 38.99, 10.0, 42),
        ("Masking Film Pre-Taped 9ftx400ft","prep",           8.00,  14.99, 2.0,  28),
    ],
    6: [  # Garden & Outdoor (20)
        ("Garden Hose 5/8in x 50ft",    "watering",     12.00, 21.99, 5.0,  85),
        ("Oscillating Sprinkler",        "watering",     8.00,  14.99, 2.0,  60),
        ("Round-Point Shovel 48in",      "digging",      14.00, 24.99, 5.0,  70),
        ("Leaf Rake 24in",              "cleanup",      8.50,  15.99, 2.5,  65),
        ("Wheelbarrow 6 cu-ft Steel",   "hauling",      55.00, 89.99, 35.0, 28),
        ("Potting Soil 2 cu-ft Bag",    "soil",         5.50,  9.99,  25.0, 88),
        ("Mulch Bag 2 cu-ft Red",       "soil",         3.50,  5.99,  30.0, 82),
        ("Lawn Fertilizer 15lb Bag",    "chemicals",    12.00, 21.99, 15.0, 58),
        ("Weed Killer Concentrate 32oz","chemicals",    9.00,  16.99, 2.0,  52),
        ("Garden Gloves Nitrile S/M/L", "protection",   4.00,  7.99,  0.3,  78),
        ("Pruning Shears Bypass 8in",   "cutting",      8.00,  14.99, 0.5,  62),
        ("Loppers 28in Bypass",         "cutting",      18.00, 32.99, 3.0,  35),
        ("Push Mower Gas 21in",         "power equip",  180.00,299.99,65.0, 12),
        ("String Trimmer Cordless 20V", "power equip",  75.00, 129.99,8.0,  25),
        ("Raised Garden Bed Kit 4x4",  "structures",   35.00, 59.99, 20.0, 38),
        ("Landscape Fabric 4x50ft",    "fabric",       12.00, 21.99, 5.0,  45),
        ("Solar Path Lights 6-Pack",   "lighting",     10.00, 19.99, 3.0,  55),
        ("Patio Umbrella 9ft",         "furniture",    42.00, 79.99, 12.0, 18),
        ("Planter Pot Ceramic 14in",   "pots",         12.00, 22.99, 8.0,  48),
        ("Tomato Cage Wire 5-Pack",    "supports",     6.00,  11.99, 3.0,  55),
    ],
    7: [  # Home Décor (15)
        ("Shelf Bracket Set Heavy-Duty","hardware",     5.00,  9.99,  2.0,  62),
        ("Floating Shelf 36in White",   "shelving",     14.00, 27.99, 4.0,  55),
        ("Curtain Rod Adjustable 72in", "window",       10.00, 19.99, 2.0,  48),
        ("Command Hooks Medium 6-Pack", "hanging",      4.50,  8.99,  0.3,  80),
        ("Picture Frame 8x10 Black",    "frames",       5.00,  11.99, 1.0,  58),
        ("Wall Mirror 24x36 Frameless", "mirrors",      28.00, 49.99, 10.0, 25),
        ("Cabinet Knobs Brushed 10-Pk", "cabinet hw",   8.00,  16.99, 1.0,  55),
        ("Entry Door Handle Set Satin", "door hw",      25.00, 44.99, 3.0,  35),
        ("Drawer Pulls 5in 5-Pack",    "cabinet hw",   7.00,  14.99, 1.0,  48),
        ("Welcome Mat Coir 18x30",      "mats",         6.00,  12.99, 3.0,  52),
        ("House Numbers 4in Black Set", "address",      4.00,  8.99,  0.3,  42),
        ("Mailbox Post-Mount Black",    "mailbox",      18.00, 34.99, 5.0,  18),
        ("Weather Stripping Foam 17ft", "sealing",      3.50,  6.99,  0.3,  55),
        ("Door Sweep Aluminum 36in",    "sealing",      5.00,  9.99,  0.5,  45),
        ("Closet Rod Chrome 6ft",       "storage",      6.00,  12.99, 2.0,  35),
    ],
    8: [  # Seasonal & Holiday (16)
        ("Space Heater Ceramic 1500W",  "heating",      28.00, 49.99, 5.0,  55),
        ("Snow Shovel Ergonomic 18in",  "winter",       15.00, 27.99, 4.0,  40),
        ("Ice Melt 50lb Jug",           "winter",       10.00, 18.99, 50.0, 45),
        ("Window Insulation Kit 5-Pack","weatherize",   5.00,  9.99,  0.5,  50),
        ("Fire Pit Steel 30in",         "outdoor",      65.00, 119.99,25.0, 20),
        ("Patio Heater Propane 48K BTU","heating",      120.00,199.99,35.0, 12),
        ("String Lights LED 100ft",     "lighting",     12.00, 24.99, 2.0,  65),
        ("Holiday Wreath 24in Pre-Lit", "holiday",      18.00, 34.99, 3.0,  42),
        ("Outdoor Ext Cord 100ft 12/3", "cords",        35.00, 59.99, 8.0,  48),
        ("Smoke Detector 2-Pack",       "safety",       12.00, 24.99, 0.5,  68),
        ("CO Detector Plug-In Digital", "safety",       15.00, 29.99, 0.5,  45),
        ("Fire Extinguisher ABC 5lb",   "safety",       22.00, 39.99, 7.0,  52),
        ("Flashlight LED 1000-Lumen",   "lighting",     10.00, 19.99, 0.5,  58),
        ("Battery Pack AA 24-Count",    "batteries",    6.00,  12.99, 1.0,  85),
        ("Portable Generator 3500W",    "power",        550.00,899.99,95.0, 5),
        ("Heavy-Duty Tarp 10x12",       "covers",       10.00, 19.99, 4.0,  55),
    ],
}

products = []
prod_id = 1
prod_by_id = {}
prod_popularity = {}
prod_by_dept = defaultdict(list)

for dept_id, items in _PRODS.items():
    supp_choices = DEPT_SUPPLIERS[dept_id]
    for i, (name, category, cost, price, weight, popularity) in enumerate(items):
        sku = f"MHH-{dept_id:02d}-{i+1:03d}"
        p = {
            "id": prod_id, "sku": sku, "name": name,
            "departmentId": dept_id,
            "supplierId": random.choice(supp_choices),
            "category": category,
            "unitCost": cost, "unitPrice": price,
            "weightLbs": weight, "popularity": popularity,
        }
        products.append(p)
        prod_by_id[prod_id] = p
        prod_popularity[prod_id] = popularity
        prod_by_dept[dept_id].append(prod_id)
        prod_id += 1

# Sorted product IDs by popularity (descending) for Pareto-weighted selection
_sorted_prods = sorted(products, key=lambda p: p["popularity"], reverse=True)
_prod_ids_sorted = [p["id"] for p in _sorted_prods]
_prod_weights = [p["popularity"] for p in _sorted_prods]

print(f"  Products: {len(products)}")

# ════════════════════════════════════════════════════════════════
# 7. Customers (500)
# ════════════════════════════════════════════════════════════════
_FIRST_NAMES = [
    "James","Mary","Robert","Patricia","John","Jennifer","Michael","Linda","David","Elizabeth",
    "William","Barbara","Richard","Susan","Joseph","Jessica","Thomas","Sarah","Christopher","Karen",
    "Charles","Lisa","Daniel","Nancy","Matthew","Betty","Anthony","Margaret","Mark","Sandra",
    "Donald","Ashley","Steven","Dorothy","Andrew","Kimberly","Paul","Emily","Joshua","Donna",
    "Kenneth","Michelle","Kevin","Carol","Brian","Amanda","George","Melissa","Timothy","Deborah",
    "Ronald","Stephanie","Edward","Rebecca","Jason","Sharon","Jeffrey","Laura","Ryan","Cynthia",
    "Jacob","Kathleen","Gary","Amy","Nicholas","Angela","Eric","Shirley","Jonathan","Anna",
    "Stephen","Brenda","Larry","Pamela","Justin","Emma","Scott","Nicole","Brandon","Helen",
    "Benjamin","Samantha","Samuel","Katherine","Raymond","Christine","Gregory","Debra","Frank","Rachel",
    "Alexander","Carolyn","Patrick","Janet","Jack","Catherine","Dennis","Maria","Jerry","Heather",
    "Wei","Mei","Yuki","Sanjay","Priya","Omar","Fatima","Aleksandr","Tatiana","Luis",
    "Sofia","Hiroshi","Nadia","Kwame","Aisha","Marco","Ingrid","Raj","Olga","Diego",
]
_LAST_NAMES = [
    "Smith","Johnson","Williams","Brown","Jones","Garcia","Miller","Davis","Rodriguez","Martinez",
    "Hernandez","Lopez","Gonzalez","Wilson","Anderson","Thomas","Taylor","Moore","Jackson","Martin",
    "Lee","Perez","Thompson","White","Harris","Sanchez","Clark","Ramirez","Lewis","Robinson",
    "Walker","Young","Allen","King","Wright","Scott","Torres","Nguyen","Hill","Flores",
    "Green","Adams","Nelson","Baker","Hall","Rivera","Campbell","Mitchell","Carter","Roberts",
    "Chen","Kim","Patel","Shah","Singh","Wang","Li","Zhang","Liu","Yang",
    "Kumar","Tanaka","Nakamura","Kowalski","Novak","Petrov","Johansson","Schmidt","O'Brien","Murphy",
]

customers = []
_cust_emails_used = set()
for cid in range(1, 501):
    fn = random.choice(_FIRST_NAMES)
    ln = random.choice(_LAST_NAMES)
    # Member since: random date between store open and END
    ms_offset = random.randint(0, (END - date(2020, 3, 15)).days)
    member_since = date(2020, 3, 15) + timedelta(days=ms_offset)
    if member_since > END: member_since = END
    # Tier: 60% bronze, 28% silver, 12% gold
    r = random.random()
    tier = "gold" if r < 0.12 else ("silver" if r < 0.40 else "bronze")
    # Spending propensity (gold buys more often)
    propensity = {"gold": 3.0, "silver": 1.5, "bronze": 1.0}[tier]
    email = gen_customer_email(fn, ln)
    while email in _cust_emails_used:
        email = gen_customer_email(fn, ln)
    _cust_emails_used.add(email)
    customers.append({
        "id": cid, "firstName": fn, "lastName": ln,
        "email": email,
        "memberSince": fmt(member_since), "tier": tier,
        "propensity": propensity,
    })

# Build customer selection weights (gold more likely to appear in sales)
_cust_weights = [c["propensity"] for c in customers]
print(f"  Customers: {len(customers)}")

# ════════════════════════════════════════════════════════════════
# 8. Accounts (chart of accounts)
# ════════════════════════════════════════════════════════════════
accounts = [
    {"id": 1,  "code":"5000","name":"Cost of Goods Sold",  "type":"COGS"},
    {"id": 2,  "code":"6010","name":"Salaries & Wages",    "type":"Payroll"},
    {"id": 3,  "code":"6020","name":"Payroll Taxes",       "type":"Payroll"},
    {"id": 4,  "code":"6030","name":"Employee Benefits",   "type":"Payroll"},
    {"id": 5,  "code":"6100","name":"Rent",                "type":"Occupancy"},
    {"id": 6,  "code":"6110","name":"Utilities",           "type":"Occupancy"},
    {"id": 7,  "code":"6120","name":"Property Insurance",  "type":"Occupancy"},
    {"id": 8,  "code":"6200","name":"Marketing & Advertising","type":"OpEx"},
    {"id": 9,  "code":"6210","name":"Office Supplies",     "type":"OpEx"},
    {"id": 10, "code":"6220","name":"Store Supplies",      "type":"OpEx"},
    {"id": 11, "code":"6300","name":"Maintenance & Repairs","type":"Facilities"},
    {"id": 12, "code":"6310","name":"Equipment Depreciation","type":"Facilities"},
    {"id": 13, "code":"6400","name":"Professional Services","type":"OpEx"},
    {"id": 14, "code":"6500","name":"Credit Card Fees",    "type":"Financial"},
    {"id": 15, "code":"6600","name":"Shrinkage & Loss",    "type":"Loss"},
]

# ════════════════════════════════════════════════════════════════
# 9. Shifts (~6000-8000)
# ════════════════════════════════════════════════════════════════
print("  Generating shifts...")
shifts = []
shift_id = 1
# Employee schedule patterns (days of week they typically work, 0=Mon)
emp_schedules = {}
for emp in employees:
    if emp["role"] in ("Store Manager",):
        days = [0,1,2,3,4]  # M-F
    elif emp["role"] in ("Assistant Manager",):
        days = random.sample(range(7), 5)
    elif emp["role"] in ("Department Lead",):
        days = random.sample(range(7), 5)
    elif emp["role"] == "Cashier":
        days = random.sample(range(7), 5)
    elif emp["role"] == "Stock Clerk":
        days = [0,1,2,3,4] if random.random() < 0.5 else random.sample(range(7), 5)
    else:
        days = random.sample(range(7), random.choice([4,4,5,5,5]))
    emp_schedules[emp["id"]] = sorted(days)

# Track which employees work each day (for sales generation)
employees_on_day = defaultdict(list)

d = START
while d <= END:
    for emp in employees:
        hire = date.fromisoformat(emp["hireDate"])
        if hire > d: continue
        if emp["terminationDate"] and date.fromisoformat(emp["terminationDate"]) < d: continue
        if d.weekday() not in emp_schedules[emp["id"]]: continue
        # Skip some days randomly (~10% absence rate)
        if random.random() < 0.08: continue

        if emp["role"] in ("Store Manager", "Assistant Manager"):
            start_h = random.choice([7,8])
            hrs = random.choice([8,9,10])
        elif emp["role"] == "Stock Clerk":
            start_h = random.choice([5,6,7])
            hrs = random.choice([6,7,8])
        elif emp["role"] == "Cashier":
            start_h = random.choice([7,8,9,10,12,13,14])
            hrs = random.choice([6,7,8])
        else:
            start_h = random.choice([7,8,9,10,12,13])
            hrs = random.choice([6,7,8])

        end_h = min(start_h + hrs, 22)
        actual_hrs = round(end_h - start_h + random.uniform(-0.25, 0.25), 2)
        if actual_hrs <= 0: continue

        shifts.append({
            "id": shift_id,
            "employeeId": emp["id"],
            "date": fmt(d),
            "startTime": f"{start_h:02d}:00",
            "endTime": f"{end_h:02d}:00",
            "hoursWorked": actual_hrs,
        })
        employees_on_day[fmt(d)].append(emp)
        shift_id += 1
    d += timedelta(days=1)

print(f"  Shifts: {len(shifts)}")

# ════════════════════════════════════════════════════════════════
# 10. Sales + Sale Items
# ════════════════════════════════════════════════════════════════
print("  Generating sales...")
sales = []
sale_items = []
sale_id = 1
item_id = 1
product_units_sold = defaultdict(lambda: defaultdict(int))  # prod_id -> {month_key -> qty}
daily_stats = {}

d = START
while d <= END:
    day_key = fmt(d)
    # How many sales today?
    base = BASE_DAILY_SALES
    mult = seasonal_mult(d) * dow_mult(d) * growth_mult(d)
    if is_black_friday(d):
        mult *= 3.0
    expected = base * mult
    num_sales = max(1, int(random.gauss(expected, expected * 0.15)))

    on_duty = employees_on_day.get(day_key, [])
    cashiers = [e for e in on_duty if e["role"] in ("Cashier", "Sales Associate", "Department Lead")]
    if not cashiers:
        cashiers = [employees[0]]  # fallback to manager

    day_revenue = 0.0
    day_items = 0
    day_customers_set = set()

    for _ in range(num_sales):
        # Time of sale (weighted toward afternoon)
        is_weekend = d.weekday() >= 5
        if is_weekend:
            hour = random.choices(range(8, 20), weights=[2,3,5,6,8,9,10,9,8,6,4,2])[0]
        else:
            hour = random.choices(range(7, 21), weights=[2,3,4,5,6,7,9,10,9,8,6,5,4,2])[0]
        minute = random.randint(0, 59)
        second = random.randint(0, 59)
        sale_time = f"{hour:02d}:{minute:02d}:{second:02d}"

        emp = random.choice(cashiers)

        # Customer (60% walk-in, 40% member)
        cust_id = None
        if random.random() < 0.40:
            cust = random.choices(customers, weights=_cust_weights, k=1)[0]
            cust_id = cust["id"]
            day_customers_set.add(cust_id)

        payment = random.choices(
            ["credit_card", "debit_card", "cash", "check"],
            weights=[50, 25, 22, 3], k=1
        )[0]

        # Number of items (1-8, weighted toward 1-3)
        num_items = random.choices(range(1, 9), weights=[28, 25, 20, 12, 7, 4, 2, 2], k=1)[0]

        subtotal = 0.0
        items_in_sale = []
        month_key = f"{d.year}-{d.month:02d}"

        for _ in range(num_items):
            # Pick product (popularity-weighted)
            pid = random.choices(_prod_ids_sorted, weights=_prod_weights, k=1)[0]
            p = prod_by_id[pid]

            # Department-seasonal boost on product selection
            dept_boost = dept_seasonal_boost(p["departmentId"], d.month)
            if random.random() > dept_boost and dept_boost < 1.0:
                # Re-pick from all products
                pid = random.choices(_prod_ids_sorted, weights=_prod_weights, k=1)[0]
                p = prod_by_id[pid]

            # Quantity
            qty = random.choices([1,1,1,2,2,3,4,5], weights=[40,20,10,12,5,5,5,3], k=1)[0]

            # Slight price variation (sales, rounding)
            unit_price = p["unitPrice"]
            if random.random() < 0.08:  # 8% chance of sale/discount
                unit_price = money(unit_price * random.uniform(0.80, 0.95))

            line_total = money(unit_price * qty)
            items_in_sale.append({
                "id": item_id,
                "saleId": sale_id,
                "productId": pid,
                "quantity": qty,
                "unitPrice": unit_price,
                "lineTotal": line_total,
            })
            subtotal += line_total
            product_units_sold[pid][month_key] += qty
            item_id += 1

        tax = money(subtotal * TAX_RATE)
        total = money(subtotal + tax)
        day_revenue += total
        day_items += len(items_in_sale)

        sales.append({
            "id": sale_id,
            "date": day_key,
            "time": sale_time,
            "employeeId": emp["id"],
            "customerId": cust_id,
            "paymentMethod": payment,
            "itemCount": len(items_in_sale),
            "subtotal": money(subtotal),
            "tax": tax,
            "total": total,
        })
        sale_items.extend(items_in_sale)
        sale_id += 1

    daily_stats[day_key] = {
        "date": day_key,
        "transactionCount": num_sales,
        "totalRevenue": money(day_revenue),
        "avgTransactionValue": money(day_revenue / num_sales) if num_sales else 0,
        "itemsSold": day_items,
        "uniqueCustomers": len(day_customers_set),
    }
    d += timedelta(days=1)

print(f"  Sales: {len(sales)}")
print(f"  Sale Items: {len(sale_items)}")

# ════════════════════════════════════════════════════════════════
# 11. Inventory Snapshots (monthly per product)
# ════════════════════════════════════════════════════════════════
print("  Generating inventory snapshots...")
inventory_snapshots = []
inv_id = 1

# Initial stock levels based on popularity
stock_levels = {}
for p in products:
    base_stock = int(p["popularity"] * 1.5 + random.randint(5, 30))
    reorder_point = max(5, int(base_stock * 0.25))
    stock_levels[p["id"]] = {"qty": base_stock, "reorder": reorder_point, "base": base_stock}

# Iterate month by month
cur = date(START.year, START.month, 1)
end_month = date(END.year, END.month, 1)
purchase_orders = []
po_id = 1

while cur <= end_month:
    month_key = f"{cur.year}-{cur.month:02d}"
    next_month = (cur.replace(day=28) + timedelta(days=4)).replace(day=1)
    snap_date = min(next_month - timedelta(days=1), END)

    for p in products:
        pid = p["id"]
        sold = product_units_sold[pid].get(month_key, 0)
        stock_levels[pid]["qty"] -= sold

        # Restock if below reorder point
        if stock_levels[pid]["qty"] < stock_levels[pid]["reorder"]:
            restock_qty = stock_levels[pid]["base"] + random.randint(5, 20)
            stock_levels[pid]["qty"] += restock_qty

            supplier = next((s for s in suppliers if s["id"] == p["supplierId"]), suppliers[0])
            order_date = cur + timedelta(days=random.randint(1, 10))
            if order_date > END: order_date = END
            recv_date = order_date + timedelta(days=supplier["leadTimeDays"] + random.randint(-1, 3))
            if recv_date > END: recv_date = END

            purchase_orders.append({
                "id": po_id,
                "supplierId": p["supplierId"],
                "productId": pid,
                "quantity": restock_qty,
                "unitCost": p["unitCost"],
                "totalCost": money(p["unitCost"] * restock_qty),
                "orderDate": fmt(order_date),
                "expectedDeliveryDate": fmt(order_date + timedelta(days=supplier["leadTimeDays"])),
                "receivedDate": fmt(recv_date),
                "status": "received",
            })
            po_id += 1

        # Ensure stock doesn't go negative
        if stock_levels[pid]["qty"] < 0:
            stock_levels[pid]["qty"] = 0

        inventory_snapshots.append({
            "id": inv_id,
            "productId": pid,
            "date": fmt(snap_date),
            "quantityOnHand": stock_levels[pid]["qty"],
            "reorderPoint": stock_levels[pid]["reorder"],
            "monthlyUnitsSold": sold,
        })
        inv_id += 1

    cur = next_month

print(f"  Inventory Snapshots: {len(inventory_snapshots)}")
print(f"  Purchase Orders: {len(purchase_orders)}")

# ════════════════════════════════════════════════════════════════
# 12. Expenses
# ════════════════════════════════════════════════════════════════
print("  Generating expenses...")
expenses = []
exp_id = 1

cur = date(START.year, START.month, 1)
while cur <= end_month:
    month_str = f"{cur.year}-{cur.month:02d}"
    exp_date = cur + timedelta(days=random.randint(0, 5))
    if exp_date > END: exp_date = END

    # Rent — fixed
    expenses.append({"id": exp_id, "date": fmt(exp_date), "accountId": 5,
        "amount": 18000.00, "vendor": "Meridian Property Group", "description": f"Monthly rent {month_str}"}); exp_id += 1

    # Utilities — seasonal
    util_base = 3200
    util_seasonal = {1:1.3,2:1.25,3:1.0,4:0.9,5:0.95,6:1.1,7:1.3,8:1.35,9:1.1,10:0.95,11:1.0,12:1.2}
    util_amt = money(util_base * util_seasonal.get(cur.month, 1.0) + random.uniform(-200, 200))
    expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=15)), "accountId": 6,
        "amount": util_amt, "vendor": "PG&E / San Jose Water", "description": f"Utilities {month_str}"}); exp_id += 1

    # Insurance — fixed
    expenses.append({"id": exp_id, "date": fmt(exp_date), "accountId": 7,
        "amount": 3500.00, "vendor": "Hartford Business Insurance", "description": f"Property & liability insurance {month_str}"}); exp_id += 1

    # Payroll — computed from shifts this month
    month_shifts = [s for s in shifts if s["date"].startswith(month_str)]
    payroll_total = sum(
        s["hoursWorked"] * next((e["hourlyRate"] for e in employees if e["id"] == s["employeeId"]), 17.0)
        for s in month_shifts
    )
    expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=1)), "accountId": 2,
        "amount": money(payroll_total), "vendor": "Internal — Payroll", "description": f"Gross wages {month_str}"}); exp_id += 1

    # Payroll taxes (~12% of payroll)
    expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=1)), "accountId": 3,
        "amount": money(payroll_total * 0.12), "vendor": "IRS / EDD", "description": f"Payroll taxes {month_str}"}); exp_id += 1

    # Benefits (~8% of payroll)
    expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=1)), "accountId": 4,
        "amount": money(payroll_total * 0.08), "vendor": "Aetna Health / 401k Admin", "description": f"Employee benefits {month_str}"}); exp_id += 1

    # Marketing — variable
    mkt_base = 1800
    mkt_seasonal = {3:1.5,4:1.4,5:1.3,11:1.5,12:2.0}
    mkt_amt = money(mkt_base * mkt_seasonal.get(cur.month, 1.0) + random.uniform(-300, 500))
    expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=10)), "accountId": 8,
        "amount": mkt_amt, "vendor": random.choice(["Google Ads","Meta Ads","Local Mailer Co","SignPro Printing"]),
        "description": f"Advertising & marketing {month_str}"}); exp_id += 1

    # Store supplies
    supplies_amt = money(random.uniform(600, 1400))
    expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=8)), "accountId": 10,
        "amount": supplies_amt, "vendor": random.choice(["Uline","Staples Business","HD Supply"]),
        "description": f"Store supplies {month_str}"}); exp_id += 1

    # Maintenance (not every month)
    if random.random() < 0.65:
        maint_amt = money(random.uniform(400, 3500))
        expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=random.randint(5,25))), "accountId": 11,
            "amount": maint_amt,
            "vendor": random.choice(["Bay Area HVAC","Quick Fix Plumbing","Electrical Solutions","CleanPro Janitorial","RoofMasters"]),
            "description": random.choice(["HVAC repair","Plumbing maintenance","Electrical inspection","Deep cleaning","Roof leak repair","Parking lot patching","Forklift service","Door repair"])}); exp_id += 1

    # Equipment depreciation
    expenses.append({"id": exp_id, "date": fmt(exp_date), "accountId": 12,
        "amount": 2100.00, "vendor": "Internal — Depreciation", "description": f"Monthly depreciation {month_str}"}); exp_id += 1

    # CC processing fees (~2.5% of credit/debit sales)
    month_sales = [s for s in sales if s["date"].startswith(month_str)]
    cc_sales = sum(s["total"] for s in month_sales if s["paymentMethod"] in ("credit_card","debit_card"))
    cc_fee = money(cc_sales * 0.025)
    expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=28)), "accountId": 14,
        "amount": cc_fee, "vendor": "Square / Visa / Mastercard", "description": f"CC processing fees {month_str}"}); exp_id += 1

    # Shrinkage (~0.3% of revenue quarterly)
    if cur.month in (3, 6, 9, 12):
        quarter_months = [f"{cur.year}-{m:02d}" for m in range(max(1,cur.month-2), cur.month+1)]
        q_rev = sum(s["total"] for s in sales if any(s["date"].startswith(qm) for qm in quarter_months))
        shrink = money(q_rev * 0.003)
        expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=25)), "accountId": 15,
            "amount": shrink, "vendor": "Internal — Inventory Adjustment",
            "description": f"Quarterly shrinkage adjustment Q{(cur.month-1)//3+1} {cur.year}"}); exp_id += 1

    # Professional services (quarterly)
    if cur.month in (1, 4, 7, 10):
        ps_amt = money(random.uniform(1500, 4000))
        expenses.append({"id": exp_id, "date": fmt(exp_date + timedelta(days=12)), "accountId": 13,
            "amount": ps_amt, "vendor": random.choice(["Morrison & Co CPA","TechAssist IT","Legal Shield LLC"]),
            "description": random.choice(["Quarterly accounting","IT support contract","Legal consultation","Tax preparation"])}); exp_id += 1

    cur = (cur.replace(day=28) + timedelta(days=4)).replace(day=1)

print(f"  Expenses: {len(expenses)}")

# ════════════════════════════════════════════════════════════════
# 13. Daily Summaries
# ════════════════════════════════════════════════════════════════
daily_summaries = sorted(daily_stats.values(), key=lambda x: x["date"])
print(f"  Daily Summaries: {len(daily_summaries)}")

# ════════════════════════════════════════════════════════════════
# Strip internal-only fields before output
# ════════════════════════════════════════════════════════════════
for c in customers:
    c.pop("propensity", None)

# ════════════════════════════════════════════════════════════════
# Assemble & Write
# ════════════════════════════════════════════════════════════════
db = {
    "_meta": {
        "database": "Meridian Hardware & Home",
        "description": "Complete relational store database with 21 months of operational data",
        "generated": fmt(END),
        "seed": SEED,
        "dateRange": {"start": fmt(START), "end": fmt(END)},
        "taxRate": TAX_RATE,
        "recordCounts": {
            "store": 1,
            "zones": len(zones),
            "departments": len(departments),
            "suppliers": len(suppliers),
            "employees": len(employees),
            "products": len(products),
            "customers": len(customers),
            "accounts": len(accounts),
            "shifts": len(shifts),
            "sales": len(sales),
            "sale_items": len(sale_items),
            "inventory_snapshots": len(inventory_snapshots),
            "purchase_orders": len(purchase_orders),
            "expenses": len(expenses),
            "daily_summaries": len(daily_summaries),
        },
    },
    "store": store,
    "zones": zones,
    "departments": departments,
    "suppliers": suppliers,
    "employees": employees,
    "products": products,
    "customers": customers,
    "accounts": accounts,
    "shifts": shifts,
    "sales": sales,
    "sale_items": sale_items,
    "inventory_snapshots": inventory_snapshots,
    "purchase_orders": purchase_orders,
    "expenses": expenses,
    "daily_summaries": daily_summaries,
}

total_records = sum(v for k, v in db["_meta"]["recordCounts"].items())
db["_meta"]["totalRecords"] = total_records

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w') as f:
    json.dump(db, f, separators=(',', ':'))

# Also write a pretty-printed version for inspection
OUT_PRETTY = OUT.replace('.json', '_pretty.json')

file_size = os.path.getsize(OUT)
print(f"\n{'='*60}")
print(f"Generated: {OUT}")
print(f"File size: {file_size:,} bytes ({file_size/1024/1024:.1f} MB)")
print(f"Total records: {total_records:,}")
print(f"{'='*60}")
for table, count in db["_meta"]["recordCounts"].items():
    print(f"  {table:.<30} {count:>8,}")
